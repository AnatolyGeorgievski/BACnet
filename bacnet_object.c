/*!
    TODO ObjectClass tree_lookup Заменить на bsearch
*/

//#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h>
#include "atomic.h"
#include "r3_tree.h"
#include "r3_asn.h"
#include "r3_object.h"
#include "bacnet.h"
#include "bacnet_error.h"

//typedef struct _BACnetNPCI BACnetNPCI;
/*
typedef struct _BACnetService BACnetService_t;
struct _BACnetService {
    int (*request)(BACnetNPCI* ncpi, uint8_t* buffer, size_t size);
};*/

extern const DeviceObjectClass* object_classes_lookup(uint16_t type_id);

#define ASSERT(v) if(!(v)) return -1;
#define ERROR(errClass, errCode) ~((errClass<<16)|errCode)

//extern uint8_t * r3_pspec_encode_param(uint8_t *buf,  void*ptr, uint16_t context_id, const ParamSpec_t* desc);
extern uint8_t * r3_pspec_decode_param(uint8_t *buf,  void*ptr, const ParamSpec_t* desc);
extern void r3_pspec_free(void* data, const ParamSpec_t *desc, int pspec_length);

typedef struct _ObjectPropertyReference ObjectPropertyReference_t;
struct _ObjectPropertyReference {
    uint32_t object_identifier;
    uint32_t/* enum _BACnetPropertyIdentifier */ property_identifier;// используются 22 бита
    uint16_t property_array_index;// можно оставить только 10 бит, тогда уложится в 32 бита
    uint16_t context_id;// индекс в таблице свойств объекта
    const PropertySpec_t * pspec;
    void* data;
};
//#define DEF(name) name##_desc, sizeof(name##_desc)/sizeof(ParamSpec_t)
//#define ARRAY_OF(type) struct { uint32_t count; type* array; }
//#define OFFSET(type, member) ((unsigned long)&((type*)0)->member)
//tree_t *device_tree=NULL;
//tree_t *object_classes=NULL;
static uint32_t object_unique_id(uint32_t type)
{
    static volatile uint32_t count=0;
    return (type<<22) | ++count;//atomic_fetch_add(&count,1);
}
/*
static const PropertySpec_t CreateObject_Request[]=  SEQUENCE {
[0] = {"object-specifier",  ASN_CLASS_CONTEXT| ASN_TYPE_CHOICE, CHOICE {
    [0] = {"object-type", ASN_TYPE_ENUMERATED}, // BACnetObjectType
    [1] = {"object-identifier", ASN_TYPE_OID}, // BACnetObjectIdentifier
    }},
[1] = {"list-of-initial-values", ASN_CLASS_SEQUENCE_OF|ASN_CLASS_CONSTRUCTIVE, BACnetPropertyValue, OPTIONAL}
};*/
typedef struct _PropertyValue PropertyValue_t;
typedef struct _PropertyReference PropertyReference_t;
struct _PropertyValue {
    uint32_t/* enum _BACnetPropertyIdentifier */ property_identifier;
    uint16_t property_array_index;
    uint16_t priority;// RANGE(1, 16)
    void* property_value;
};
struct _PropertyReference {
    uint32_t/* enum _BACnetPropertyIdentifier */ property_identifier;
    uint16_t property_array_index;
};

// Общие свойства всех объектов
static const PropertySpec_t CommonObject[] = {
    {OBJECT_IDENTIFIER,     ASN_TYPE_OID,       OFFSET(Object_t, object_identifier)},
    {OBJECT_NAME,           ASN_TYPE_STRING,    OFFSET(Object_t, object_name)},
};



/*! \return индекс в таблице свойств объекта object_class->pspec[i] */
static const int r3_property_find(const PropertySpec_t* pspec, uint32_t pspec_length, uint16_t property_identifier)
{
    uint32_t i;
    for (i=0; i<pspec_length; i++){
        if (pspec[i].prop_id == property_identifier) {
            return i;
        }
    }
    return -1;
}
static uint32_t decode_oid(uint8_t ** buffer)
{
    uint8_t* buf = *buffer;
    *buffer+=5;
    return __builtin_bswap32(*(uint32_t*)&buf[1]);
}
//__attribute__((noinline))
static uint32_t decode_unsigned(uint8_t ** buffer)
{
    uint8_t* buf = *buffer;
    int size = *buf++ & 0x7;
    *buffer=buf+size;
    uint32_t value=__builtin_bswap32(*(uint32_t*)buf);
    switch (size){
    case 3: value>>= 8; break;
    case 2: value>>=16; break;
    case 1: value>>=24; break;
    case 0: value  = 0; break;
    default: break;
    }
    return value;
}
/// TODO заменить на функции из r3asn
//static inline
static uint8_t* encode_unsigned(uint8_t * buf, uint8_t asn_type, uint32_t value)
{
    int size = value!=0? 4-(__builtin_clz(value)>>3) : 0;
    *buf++= asn_type | size;
    switch (size){
    case 4: *(uint32_t*)buf=__builtin_bswap32(value);buf+=4; break;
    case 3: *buf++=value>>16;
    case 2: *buf++=value>>8;
    case 1: *buf++=value;
    default: break;
    }
    return buf;
}
static uint8_t* encode_32(uint8_t * buf, uint8_t asn_type, uint32_t value)
{
    *buf++ = asn_type;
    *(uint32_t*)buf = __builtin_bswap32(value); buf+=4;
    return buf;
}
extern uint8_t* r3_pspec_decode(uint8_t* buf, void* data, const ParamSpec_t* desc, int desc_length);
/// FIX r3_pspec_decode_param - заменить на эту, в файле r3_asn
static uint8_t * _pspec_decode_param(uint8_t *buf,  void*ptr, uint16_t context_id, const PropertySpec_t* pspec)
{
    const PropertySpec_t* desc = pspec+context_id;
	uint16_t node_tag;
	size_t   node_length;
	buf = r3_decode_tag(buf, &node_tag, &node_length);
    if ((desc->asn_type&ASN_CLASS_MASK)==ASN_CLASS_SEQUENCE) {// SEQUENCE
        buf = r3_pspec_decode (buf, ptr, desc->ref, desc->size);
		if ((buf[0] & ASN_CLASS_MASK)==ASN_CLASS_CLOSING)
			buf = r3_decode_tag(buf, &node_tag, &node_length);
    } else
    if ((desc->asn_type&ASN_CLASS_MASK)==ASN_CLASS_SEQUENCE_OF) {// SEQUENCE OF
		List * prev = *(List**)ptr;
		if (prev) while(prev->next) prev = prev->next; // в конец списка добавляем
		int size = (desc->asn_type>>8) + sizeof(List*);
		while((node_tag & ASN_CLASS_MASK)!=ASN_CLASS_CLOSING) {
			List* elem = g_slice_alloc(size);
			elem->next = NULL;
			buf = r3_pspec_decode(buf, elem->data, desc->ref, desc->size);
			if(prev) prev->next = elem;
			else *(List**)ptr = elem;
			prev = elem;
			node_tag = buf[0];
		}
		buf = r3_decode_tag(buf, &node_tag, &node_length);// закрывающий тег конструктивного типа
    } else
	{ // не конструктивно
		switch (desc->asn_type&0xF0) {
        case ASN_TYPE_ANY: // 0xE0
            *(void**)ptr = buf;
            break;
		case ASN_TYPE_NULL: break;  // 0x00
		case ASN_TYPE_CHOICE: break;
		case ASN_TYPE_BOOLEAN:{     // 0x10
			if (node_tag & ASN_CLASS_CONTEXT) {
				*(bool *)ptr = (*buf!=0)?true:false;
			} else {
				*(bool *)ptr = (node_tag & 1)?true:false;
				node_length = 0;
			}
		} break;
		case ASN_TYPE_ENUMERATED:   // 0x90
		case ASN_TYPE_UNSIGNED: {   // 0x20
			uint32_t value;
			switch (node_length){// способ записи определен как BE
			case 0: value = 0; break;
			case 1: value = buf[0]; break;
			case 2: value = __builtin_bswap16(*(uint16_t*)buf); break;
			case 3: value = (uint32_t)buf[0]<<16 | (uint32_t)buf[1]<<8 | (uint32_t)buf[2]; break;
			case 4: value = __builtin_bswap32(*(uint32_t*)buf); break;
			default:value = 0; break;
			}
			switch (desc->asn_type&0x07) {// способ записи может быть LE или BE
			case 1: *(uint8_t *)ptr = value; break;
			case 2: *(uint16_t*)ptr = value; break;
			default:// если не указано явно, то 4 байта
			case 4: *(uint32_t*)ptr = value; break;
			}
		} break;
		case ASN_TYPE_INTEGER: {    // 0x30
			int32_t value;
			switch (node_length){// способ записи определен как BE
			case 0: value = 0; break;
			case 1: value = (int8_t)buf[0]; break;
			case 2: value = (int16_t)__builtin_bswap16(*(uint16_t*)buf); break;
			case 3: value = (int32_t)buf[0]<<16 | (uint32_t)buf[1]<<8 | (uint32_t)buf[2]; break;
			case 4: value = (int32_t)__builtin_bswap32(*(uint32_t*)buf); break;
			default:value = 0; break;
			}
			value = __builtin_bswap32(*(uint32_t*)buf);
			switch (desc->asn_type&0x07) {// способ записи может быть LE или BE
			case 1: *(int8_t *)ptr = value; break;
			case 2: *(int16_t*)ptr = value; break;
			default:// если не указано явно, то 4 байта
			case 4: *(int32_t*)ptr = value; break;
			}
		} break;
		case ASN_TYPE_DOUBLE: {     // 0x50
			if (node_length==8)
				*(uint64_t*)ptr = __builtin_bswap64(*(uint64_t*)buf);
			break;
		} break;
		case ASN_TYPE_OCTETS: {     // 0x60
			uint8_t *octets = ptr;
			int size = (desc->size < node_length? desc->size : node_length);
			int i;
			for (i=0;i<size; i++) {
				octets[i] = buf[i];
			}
			size = desc->size;
			for (;i<size; i++) {
				octets[i] = 0;
			}
		} break;
		case ASN_TYPE_STRING: {     // 0x70
			// если строка в словаре, кварк?
			// строка в указателе?
			// если строка только для чтения, инициализируется из флеш памяти (SEGMENT(buf)==iFLASH) то установим ссылку
			if (node_length==0) {
				*(char**)ptr = NULL;
			} else {
				uint8_t *str = malloc(node_length);//*(uint8_t**)ptr;// сылка на строку
				// в первом байте содержится кодировка 00- UTF-8
				__builtin_memcpy(str, buf+1, node_length-1);
				str[node_length-1]='\0';
                printf(" == str = '%s'\n", str);
				str = (uint8_t*)atomic_pointer_exchange((void**)ptr, str);
				if (str!=NULL/* (SEGMENT(str)!=iFLASH */) {
					free(str);
				}
			}
		} break;
		case ASN_TYPE_BIT_STRING: { // 0x80
			if (node_length>0) {// битовая строка кодируется как число бит в последнем октете
			/*  The initial octet shall encode, as an unsigned binary integer, the number of unused bits in the final subsequent octet.
				The number of unused bits shall be in the range zero to seven, inclusive. */
				uint8_t* bits = ptr;
				uint8_t mask = 0xFF>>buf[0];
				int size = (desc->size<node_length?desc->size:node_length-1)-1;
				int i;
				for (i=0;i<size-1; i++) {
					bits[i] = buf[i+1];
				}
				bits[i] = buf[i+1] & mask; // Undefined bits shall be zero
				size = desc->size;
				for (++i;i<size; i++) {
					bits[i] = 0;
				}
			}
		} break;
		case ASN_TYPE_REAL:         // 0x40
		case ASN_TYPE_DATE:         // 0xA0
		case ASN_TYPE_TIME:         // 0xB0
		case ASN_TYPE_OID :         // 0xC0
		default: // REAL DATE TIME OID
			if (node_length==4) {
                uint32_t value = __builtin_bswap32(*(uint32_t*)buf);
				*(uint32_t*)ptr = value;
				printf(" == value = %08x\n", value);
			} else {
			    printf(" == type = %02X, size = %d\n", desc->asn_type, (int)node_length);
			}
			break;
		}
		buf+=node_length;
    }
    return buf;
}

/*! \brief выполнить кодирование базового типа
 */
uint8_t * _pspec_encode_param(uint8_t *buf,  void*ptr, uint16_t context_id, const PropertySpec_t* desc)
{
    uint16_t asn_type = desc[context_id].asn_type;
    uint16_t node_tag = asn_type;
    if (node_tag & ASN_CLASS_CONTEXT) {
        node_tag = (context_id>=0xF)?(context_id<<8)|0xF8: (context_id<<4)|0x08;
    }
#if 0
    if ((desc->asn_type&ASN_CLASS_MASK)==ASN_CLASS_SEQUENCE) {// SEQUENCE
        if (node_tag & ASN_CLASS_CONTEXT)
            buf = r3_encode_constructed(buf, node_tag|ASN_CLASS_OPENING);
        buf = r3_pspec_encode (buf, ptr, desc->ref, desc->size);
        if (node_tag & ASN_CLASS_CONTEXT)
            buf = r3_encode_constructed(buf, node_tag|ASN_CLASS_CLOSING);
    } else
    if ((desc->asn_type&ASN_CLASS_MASK)==ASN_CLASS_SEQUENCE_OF) {// SEQUENCE OF
        if (node_tag & ASN_CLASS_CONTEXT)
            buf = r3_encode_constructed(buf, node_tag|ASN_CLASS_OPENING);
        List* list = *(List**) ptr;
        while (list){
            buf = r3_pspec_encode (buf, list->data, desc->ref, desc->size);
            list = list->next;
        }
        if (node_tag & ASN_CLASS_CONTEXT)
            buf = r3_encode_constructed(buf, node_tag|ASN_CLASS_CLOSING);
    } else
#endif
    {// не конструктивно
        size_t node_length = asn_type & 0x7;
        union {
            int32_t i;
            uint32_t u;
        } value = {.u=0};
        switch(asn_type & 0xF0) {
        case ASN_TYPE_NULL:
            node_length = 0;
            break;
        case ASN_TYPE_BOOLEAN: {// 0x10
            value.u = *(bool*)ptr;
            node_length = ((node_tag&ASN_CLASS_CONTEXT) /*|| node->value.b*/)? 1: 0;
        } break;
        case ASN_TYPE_ENUMERATED:// 0x90
        case ASN_TYPE_UNSIGNED:// 0x20
            switch (node_length){
            case 1: value.u = *(uint8_t *)ptr;  break;
            case 2: value.u = *(uint16_t*)ptr;  break;
            default:
            case 4: value.u = *(uint32_t*)ptr;  break;
            }
            node_length = value.u!=0? 4-(__builtin_clz(value.u)>>3) : 0;
            break;
        case ASN_TYPE_INTEGER: {// 0x30
            switch (node_length){
            case 1: value.i = *(int8_t *)ptr;  break;
            case 2: value.i = *(int16_t*)ptr;  break;
            default:
            case 4: value.i = *(int32_t*)ptr;  break;
            }
            if (value.i<0) {
                node_length = ~value.u!=0? 4-((__builtin_clz(~value.u)-1)>>3) : 1;
            } else {
                node_length = value.u!=0? 4-((__builtin_clz(value.u)-1)>>3) : 0;
            }
        } break;
        case ASN_TYPE_OCTETS://0x60
			if (node_length==0) node_length = desc[context_id].size;
			uint8_t *s=*(uint8_t**) ptr;
            while (node_length!=0 && s[node_length-1]!=0) node_length--;
            break;
        case ASN_TYPE_STRING: {//0x70
            node_length = 0;//strlen(*(char**)ptr)+1;
            char* s=*(char**)ptr;
            if (s) {
                while(s[node_length++]!='\0');
                //node_length++;// ноль копируем?
                printf (" == String: %s\n", s);
                ptr = *(char**)ptr;
            } else {
                printf ("String String\n");
                ptr = "Unknown";
                node_length=8;
            }
        } break;
        case ASN_TYPE_BIT_STRING: { //0x80
            uint8_t* bits=(uint8_t*)ptr+node_length;
            while (node_length) {
                if (*--bits) break;
                node_length--;
            }
            node_length++;
        } break;
        default:
        case ASN_TYPE_OID:  // 0xE0
        case ASN_TYPE_TIME: // 0xB0
        case ASN_TYPE_DATE: // 0xA0
        case ASN_TYPE_REAL: // 0x40
            node_length = 4;
            break;
        case ASN_TYPE_DOUBLE:// 0x50
            node_length = 8;
            break;
        }
// кодировать тег
        buf = r3_encode_tag(buf, node_tag, node_length);
// кодировать данные

        switch(asn_type & 0xF0) {
        case ASN_TYPE_OCTETS:
            __builtin_memcpy(&buf[0], ptr, node_length);
            buf+=node_length;
            break;
        case ASN_TYPE_STRING:
            buf[0] = 0;// encoding UTF-8
            __builtin_memcpy(&buf[1], ptr, node_length-1);
            buf+=node_length;
            break;
        case ASN_TYPE_BIT_STRING: {
            uint8_t *bits = ptr;
            buf[0]  = __builtin_clz(bits[node_length-2])-24;// = unused_bits 0..7;
            __builtin_memcpy(&buf[1], ptr, node_length-1);
            buf+=node_length;
        } break;
        default:
            while (node_length>=4) {
                node_length-=4;
                *(uint32_t*)buf= __builtin_bswap32(*(uint32_t*)(ptr+node_length)); buf+=4;
            }
            if (node_length>=2) {
                node_length-=2;
                *(uint16_t*)buf = __builtin_bswap16(*(uint16_t*)(ptr+node_length)); buf+=2;//value.u>>16;
            }
            if (node_length>0) {
                *buf++ = *(uint8_t*)ptr;
            }
            break;
        }
    }
    return buf;
}

/// TODO регистрация классов? Конструктор
/*! \brief выполнить регистрацию статических объектов

*/
int r3cmd_object_init (tree_t** device_tree, Object_t* object, uint32_t object_identifier)
{
    tree_t *leaf = g_slice_alloc(sizeof(tree_t));
	tree_init(leaf, object_identifier, object);
	if ((*device_tree)==NULL)
        *device_tree = leaf;
	else
        tree_insert_tree(device_tree, leaf);
	object->object_identifier = object_identifier;
    //object->instance.object_type= object_identifier>>22;
    //if (object->instance.object_name==NULL) object->instance.object_name = "Unknown";
    if (object->property_list.array==NULL) {
        const DeviceObjectClass *object_class = object_classes_lookup(object_identifier>>22);
        if (object_class==NULL)
            return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);
        object->property_list.array = object_class->pspec;
        object->property_list.count = object_class->pspec_length;
    }
    return 0;
}
/*! \brief создать и выполнить регистрацию объекта */
int r3cmd_object_create(tree_t** device_tree, uint8_t * buffer, size_t length, uint8_t* response_buffer)
{
    uint8_t *buf = buffer;
    uint32_t object_identifier = 0x3FFFFF;
    uint32_t type_id = 0;
    ASSERT(*buf++ == 0x0E);// choice
    if ((*buf & 0xF8)  == 0x08){// enumerated
        type_id = decode_unsigned(&buf);
        object_identifier = (type_id<<22)|0x3FFFFF;
    } else {// OID
        ASSERT(*buf == 0x1C);
        object_identifier = decode_oid(&buf);
        type_id = object_identifier>>22;
    }
    ASSERT(*buf++ == 0x0F);

    const DeviceObjectClass *object_class = object_classes_lookup(type_id);
    if (object_class==NULL)
        return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);

    if ((object_identifier&OBJECT_ID_MASK)==UNDEFINED)
         object_identifier = object_unique_id(type_id);
    void* object = malloc(object_class->size);
    __builtin_bzero(object, object_class->size);

    ASSERT(*buf++ == 0x1E);// PD Opening Tag 1 (List Of Initial Values)
    const PropertySpec_t * pspec = object_class->pspec;
    uint16_t pspec_length = object_class->pspec_length;
    while (*buf   != 0x1F){
        ASSERT((*buf & 0xF8) == 0x08);// SD Context Tag 0 (Property Identifier, L=1)
        uint32_t property_identifier = decode_unsigned(&buf);
        uint16_t context_id = r3_property_find(pspec, pspec_length, property_identifier);
        if (context_id==0xFFFF) {
            context_id = r3_property_find(CommonObject, sizeof(CommonObject)/sizeof(const PropertySpec_t), property_identifier);
        }
//        buf = r3_pspec_decode(buf, &value, DEF(PropertyValue));
        void* data = object + pspec[context_id].offset;
/*
        if (*buf & 0xF8 == 0x18) { // элемент массива
            uint16_t property_array_index = decode_unsigned(&buf);
            uint32_t size;
            if (prop->asn_type&0x7) {
                size = prop->asn_type&0x7;
                if (size==0x5) size = prop->asn_type>>8;
            } else size = 4;
            data = data+size*property_array_index;
        } */
        ASSERT(*buf++ == 0x2E);// PD Opening Tag 2 (Value)
        buf = r3_pspec_decode_param(buf, data, pspec[context_id].ref);
        ASSERT(*buf++ == 0x2F);

    }
    buf++;// 0x1F
    r3cmd_object_init(device_tree, object, object_identifier);
    buf = encode_32(response_buffer, ASN_TYPE_OID|4, object_identifier);
    return buf - response_buffer;
}
#if 0
/* сформировать упакованный вид */
static void serialize(tree_t* device_tree, uint32_t object_identifier, void* user_data)
{
    uint8_t *buf = *(uint8_t**)user_data;
    buf = encode_32(buf, 0x0C, object_identifier);
	void *object = tree_lookup(device_tree, object_identifier);
	if (object != NULL) {
        uint16_t type_id = object_identifier>>22;
        DeviceObjectClass *object_class = tree_lookup(object_classes,type_id);
        const PropertySpec_t* pspec = object_class->pspec;
        if (pspec) {
            *buf++ = 0x1E;
            int i;
            for (i=0;i< object_class->pspec_length;i++){
                void *data = object+pspec[i].offset;
                //buf = r3_encode_primitive(buf, &pspec[i].prop_id, CONTEXT(1)|ASN_TYPE_ENUMERATED, 2);
                buf = encode_unsigned(buf, 0x28, pspec[i].prop_id);
                *buf++=0x4E;
/*
                if ((pspec[i].asn_type & ASN_CLASS_MASK) == ASN_CLASS_ARRAY) {// ARRAY
                    uint32_t size = pspec[i].size;// откуда взять размер элемента массива?
                    if(size==0) size = 4;
                    uint32_t length = PROPERTY_ARRAY_SIZE(pspec[i]);//pspec[i].asn_type>>8;// & 0xFF;
                    int n;
                    for(n=0;n<length;n++){//
                        buf = r3_pspec_encode_param(buf, data+n*size, pspec[i].ref);
                    }
                } else */
                {
                    buf = r3_pspec_encode_param(buf, data, pspec[i].ref);// OID, REAL, UNSIGNED,INTEGER
                }
                *buf++=0x4F;
            }
            *buf++ = 0x1F;
        }
	}
	*(uint8_t**)user_data = buf;
}

// ReinitializeDevice WARMSTART, COLDSTART, ACTIVATE_CHANGES - записать изменения во флеш и применить отложенные нстройки сетевых интерфейсов,
int r3_device_tree_backup(uint8_t * buffer, int length)
{// записать изменения во флеш и применить отложенные нстройки сетевых интерфейсов
    tree_notify(device_tree, serialize, &buffer);
    return 0;
}
#endif // 0
/*! \brief удалить объект из реестра */
int r3cmd_object_delete(tree_t** device_tree, uint8_t * buffer, size_t length)
{
    uint8_t *buf = buffer;
    ASSERT(*buf == 0x0C);
    uint32_t object_identifier = decode_oid(&buf);

	tree_t *leaf = tree_remove(device_tree, object_identifier);
	if (leaf != NULL) {
        const DeviceObjectClass *object_class = object_classes_lookup(object_identifier>>22);
        if (object_class==NULL)
            return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);

        const PropertySpec_t* pspec = object_class->pspec;

        r3_pspec_free(leaf->value, pspec->ref, pspec->size);
        free(leaf->value); // leaf->data=0;
		g_slice_free1(sizeof(tree_t), leaf);
		return 0;
	}
	return ERROR(OBJECT, UNKNOWN_OBJECT);
}
static Object_t* _object_new(uint16_t type_id)
{
    const DeviceObjectClass *object_class = object_classes_lookup(type_id);
    if (object_class==NULL)
        return NULL;

    void* object = malloc(object_class->size);
    if (object) __builtin_bzero(object, object_class->size);
    return object;
}
/*

    uint16_t asn_type;
    uint16_t offset;
    uint16_t flags;
    uint16_t size;
    const struct _ParamSpec *ref;
*/
static int r3cmd_property_reference (tree_t** device_tree, ObjectPropertyReference_t *pref, int hint)
{
    uint16_t type_id = pref->object_identifier>>22;
//
    Object_t* object = tree_lookup(*device_tree, pref->object_identifier);
    if (object==NULL) {
        if (!hint)
            return ERROR(OBJECT, UNKNOWN_OBJECT);
        object = _object_new(type_id);
        if (object==NULL)
            return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);
        int result = r3cmd_object_init(device_tree, object, pref->object_identifier);
        if (result!=0)
            return result;
//        printf("r3cmd_property_reference: ERROR(OBJECT, UNKNOWN_OBJECT)=0x%08X\n", pref->object_identifier);
    }
    const PropertySpec_t *pspec = object->property_list.array;
    uint32_t pspec_length       = object->property_list.count;
    if (pspec==NULL) {
        const DeviceObjectClass *object_class = object_classes_lookup(type_id);
        if (object_class==NULL)
            return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);
        pspec = object_class->pspec;
        pspec_length = object_class->pspec_length;
    }

    uint32_t prop_id = pref->property_identifier;
//    printf("%s: prop_id =%d\n", __FUNCTION__, prop_id);
    int i;
    for (i=0; i< pspec_length; i++){
        if (pspec[i].prop_id == prop_id) break;
    }
    if (i==pspec_length){
        pspec = CommonObject;
        pspec_length = sizeof(CommonObject)/sizeof(const PropertySpec_t);
        for (i=0; i< pspec_length; i++){
            if (pspec[i].prop_id == prop_id) break;
        }
        if (i==pspec_length){
//            printf("%s: ERROR(PROPERTY, UNKNOWN_PROPERTY)\n", __FUNCTION__, prop_id, pspec_length);
            return ERROR(PROPERTY, UNKNOWN_PROPERTY);
        }
    }
    pref->context_id = i;// номер свойства по списку, чтобы быстро находить
    void* data = (void*)object + pspec[i].offset;

    if (pspec[i].size) {// вычесть единицу
        if (pref->property_array_index>=PROPERTY_ARRAY_SIZE(&pspec[i]))
            return ERROR(PROPERTY, INVALID_ARRAY_INDEX);
        //int size = decode_size(prop[i].asn_type);
        data = data + pref->property_array_index*pspec[i].size;
    }
    pref->pspec  = pspec;//.ref;
    pref->data   = data;
    return 0;
}

/*! Функции для работы с файловой системой */
/*! \brief функция записи данных в файл (журнал)

если секция 1(1E-1F) отсуствует, транзакция расценивается как object-delete, иначе object-modify. Если объект не найден, то object-create
Псевдо код потоковой записи во флеш память.
    asn_encode(CONTEXT_TYPE(0), ObjectIdntifier); -- кодируется тип и номер объекта(число 32 бита)
    asn_begin (CONTEXT_TYPE(1)); -- описание параметров
        asn_encode(CONTEXT_TYPE(0), param->PropertyIdentifier);
        if (is_array(param))  {// если запись ведется по элементам списка или массива
            asn_encode(CONTEXT_TYPE(1), param->ArrayIndex);
        }
        if (is_default(param->value)) {// значение по умолчанию
            asn_begin (CONTEXT_TYPE(2));
                asn_encode(CONTEXT_TYPE(0), param->value);
            asn_end   (CONTEXT_TYPE(2));
        }
    asn_end   (CONTEXT_TYPE(1));
    asn_encode(CONTEXT_TYPE(3), crc32);// от блока данных (0) и (1)
*/
extern uint32_t bacnet_crc32k(uint8_t * data, size_t length);
uint8_t * r3cmd_object_store (uint8_t *buf, void* object, uint32_t object_id, const PropertySpec_t *pspec, size_t pspec_len)
{
    uint8_t *block = buf;

    buf = r3_asn_encode_u32(buf, ASN_CONTEXT(0), object_id);// это надо делать снаружи
    if (object==NULL){// объект удален!
        return buf;
    }
    *buf++ = 0x1E;
    int i;
    for (i=0; i< pspec_len; i++){
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(0), pspec[i].prop_id);
        if (0){// ARRAY_ELEMENT?
            buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), 0);
        }
        *buf++ = 0x2E;
         buf = _pspec_encode_param(buf, object + pspec[i].offset, i, &pspec[i]);
//         buf = r3_pspec_encode_param (buf, object + pspec[i].offset, /* pspec->asn_type */i, pspec[i].ref);
        *buf++ = 0x2F;
    }
    *buf++ = 0x1F;
    // может быть вынести функцию за пределы блока, блока применяется только если указана CRC или CMAC/HMAC
    uint32_t crc32 = bacnet_crc32k(block, buf-block);
    *buf++ = 0x3C;// CRC32
    *(uint32_t*)buf = crc32;
//    buf = r3_asn_encode_u32(buf, ASN_CONTEXT(3), crc32);
    return buf;
}
/*! \brief читает с носителя */
uint8_t * r3cmd_object_load (uint8_t *buf, void* object, const PropertySpec_t *pspec, size_t pspec_len)
{
    // объект читаем снаружи
    int i=0;// номер свойства в таблице, при записи некоторые свойства могут быть пропущены, тогда им присваиваются значения по умолчанию
    if (*buf != 0x1E) return buf;
    buf++;
    while (*buf != 0x1F) {
        uint32_t prop_id = 85;//PRESENT_VALUE
        uint16_t array_idx=0;
        if ((*buf & 0xF8) == ASN_CONTEXT(0)) {
            buf++;
            prop_id  = decode_unsigned(&buf);
        }
        if ((*buf & 0xF8) == ASN_CONTEXT(1)) {
            buf++;
            array_idx= decode_unsigned(&buf);
        }
        for (; i<pspec_len;i++) {
            if (pspec[i].prop_id==prop_id) break;
        }
        if (i<pspec_len && *buf == 0x2E) {
            buf++;
            buf = r3_pspec_decode_param (buf, object + pspec[i].offset, /* pspec->asn_type i, */pspec[i].ref);
            if (!(*buf == 0x2F)) break;// FIX не уверен что это правильный выход
        } else {
            buf = r3_asn_next(buf);// пропустить текущую запись
        }
    }
    buf++;
    return buf;
}

/*! \brief функция загрузки журнала
    \return возвращаем начало блока, который не был разобран.
*/
uint8_t * r3cmd_load (tree_t** device_tree, uint8_t *buf, size_t length)
{
    uint8_t* block= buf;
    uint32_t object_identifier;
    uint8_t  code = *buf;
    while (((code = *buf)&0xF)!=0xF)
    {
        switch (code ){
        case 0x0C: // 0x18 object_ID
            object_identifier = decode_oid(&buf);
            uint16_t type_id = object_identifier>>22;
            const DeviceObjectClass *object_class = object_classes_lookup(type_id);

            void* object = tree_lookup(*device_tree, object_identifier);
            if (object==NULL) {// создать объект
                object = malloc(object_class->size);
                __builtin_bzero(object, object_class->size);
                r3cmd_object_init (device_tree, object, object_identifier);
            } else {// обновить значения параметров для существующего объекта
            }
            buf = r3cmd_object_load(buf, object, object_class->pspec, object_class->pspec_length);
            break;
        case 0x3C: {// CRC32K
            buf++;
            uint32_t crc32 = bacnet_crc32k(block, buf-block);
            if (crc32 != *(uint32_t*)buf) return block;
            buf+=4;
            block = buf;
        } break;
        default:
            buf = r3_asn_next(buf);// пропустить непонятную запись
            break;
        }
    }
    return block;
}

/*! \brief обработка запроса на чтение свойства объекта */
int r3cmd_read_property(tree_t** device_tree, uint8_t * buffer, size_t length, uint8_t *response_buffer)
{
    uint8_t* buf = buffer;
    ObjectPropertyReference_t pref;
    ASSERT(*buf==0x0C);
    pref.object_identifier = decode_oid(&buf);
    ASSERT((*buf & 0xF8) == 0x18);
    pref.property_identifier = decode_unsigned(&buf);
    if ((*buf & 0xF8) == 0x28){
        pref.property_array_index = decode_unsigned(&buf);
    }
//    uint8_t *buf = r3_pspec_decode(buffer, &pref, DEF(ObjectPropertyReference));
    int result = r3cmd_property_reference (device_tree, &pref, 0);
    if (result) {
        //printf("-> Result: NF\n");
        return result;
    }
    /* формирование ответа, дописываем */
#if 0
    if(pref.pspec!=NULL && (pref.pspec->flags & RD)==0)
        return ERROR(PROPERTY, READ_ACCESS_DENIED);
#endif // 0
    printf("%s:Property ref\n", __FUNCTION__);
    __builtin_memcpy(response_buffer, buffer, buf-buffer);
    uint8_t *rsp = response_buffer+(buf-buffer);
    *rsp++=0x3E;
    rsp = _pspec_encode_param(rsp, pref.data, pref.context_id, pref.pspec);
    //buf = r3_pspec_encode_param(buf, pref.data, pref.context_id, pref.pspec);
    *rsp++=0x3F;
    printf("%s: Param encode done\n", __FUNCTION__);
    return rsp - response_buffer;
}
/*! \brief обработка запроса на изменение свойства объекта

ReadProperty-ACK ::= SEQUENCE {
    object-identifier       [0] BACnetObjectIdentifier,
    property-identifier     [1] BACnetPropertyIdentifier,
    property-array-index    [2] Unsigned OPTIONAL, -- used only with array datatype
        -- if omitted with an array the entire array is referenced
    property-value          [3] ABSTRACT-SYNTAX.&Type
}
*/
int r3cmd_write_property(tree_t** device_tree, uint8_t * buffer, size_t length, int hint)
{
    uint8_t* buf = buffer;
    ObjectPropertyReference_t pref;
    printf(" == %s:%d !!!\n", __FUNCTION__, __LINE__);
    ASSERT(*buf==0x0C);
    pref.object_identifier = decode_oid(&buf);
    ASSERT((*buf & 0xF8) == 0x18);
    printf(" == %s:%d !!!\n", __FUNCTION__, __LINE__);
    pref.property_identifier = decode_unsigned(&buf);
    if ((*buf & 0xF8) == 0x28){
        pref.property_array_index = decode_unsigned(&buf);
    }
//    uint8_t *buf = r3_pspec_decode(buffer, &pref, DEF(ObjectPropertyReference));
    int result = r3cmd_property_reference (device_tree, &pref, hint);
    if (result) {
        return result;
    }
#if 0 ///TODO
    if ((pref.pspec->flags & WR)==0)
        return ERROR(PROPERTY, WRITE_ACCESS_DENIED);
#endif // 0
    ASSERT(*buf++==0x3E);
    printf(" == %s:%d Decode start prop=%d!!!\n", __FUNCTION__, __LINE__, pref.property_identifier);
    buf = _pspec_decode_param(buf, pref.data, pref.context_id, pref.pspec);
//    buf = r3_pspec_decode_param(buf, pref.data, pref.pspec->ref);
    printf(" == %s:%d Decode done!!!\n", __FUNCTION__, __LINE__);
    ASSERT(*buf++==0x3F);// r3_encode_context(3, ASN_CLASS_CLOSING);
    printf(" == %s:%d Done !!!\n", __FUNCTION__, __LINE__);
/*
    uint8_t priority = 0;
    if((*buf&0xF8) == 0x48){
        priority = decode_unsigned(&buf);
    } */
    // дописать update object->notify(, priority);
    return buf-buffer;
}
/*! \brief обработка запроса на чтение свойств объекта
WritePropertyMultiple-Request ::= SEQUENCE {
    list-of-write-access-specifications SEQUENCE OF WriteAccessSpecification
}
ReadAccessSpecification ::= SEQUENCE {
    object-identifier [0] BACnetObjectIdentifier,
    list-of-property-references [1] SEQUENCE OF BACnetPropertyReference
}
\see F.3.7 Encoding for Example E.3.7 - ReadPropertyMultiple Service
*/
int r3cmd_read_property_multiple(tree_t** device_tree, uint8_t * buffer, size_t length, uint8_t *response_buffer)
{
    uint8_t *buf = buffer;
    uint8_t *rsp = response_buffer;
    //while (buf-buffer<length)
    if (1) {
        ASSERT(*buf == 0x0C);
        uint32_t object_identifier = decode_oid(&buf);

        Object_t* object = tree_lookup(*device_tree, object_identifier);
        if (object==NULL)
            return ERROR(OBJECT, UNKNOWN_OBJECT);
        const PropertySpec_t * pspec = object->property_list.array;
        uint16_t pspec_length = object->property_list.count;
        if (pspec == NULL) {
            const DeviceObjectClass *object_class = object_classes_lookup(object_identifier>>22);
            if (object_class==NULL)
                return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);
            pspec = object_class->pspec;
            pspec_length = object_class->pspec_length;
        }
//printf("%s: obj_id=%08X\n", __FUNCTION__, object_identifier);
        ASSERT(*buf++ == 0x1E);
//        buf = r3_decode_constructed(buf, ASN_CLASS_OPENING);
        rsp = encode_32(rsp, ASN_CONTEXT(0), object_identifier);
        *rsp++ = 0x1E;
        const PropertySpec_t * InObject = pspec;
        while (*buf != 0x1F) {
            uint8_t *hdr = buf;// начало заголовка
            ASSERT((*buf & 0xF8) == 0x08);
            uint32_t property_identifier = decode_unsigned(&buf);
            //printf("%s: - prop_id=%d\n", __FUNCTION__, property_identifier);
            rsp = encode_unsigned(rsp,  ASN_CONTEXT(2), property_identifier);
            uint32_t property_array_index = ~0;
            if (*buf!=0x1F && (*buf & 0xF8) == 0x18){// array_index -- if omitted with an array the entire array is referenced
                property_array_index =decode_unsigned(&buf);
                rsp = encode_unsigned(rsp,  ASN_CONTEXT(3), property_array_index);
            }
            pspec = InObject;
            uint16_t context_id = r3_property_find(pspec, pspec_length, property_identifier);
            if (context_id==0xFFFF){
                pspec = CommonObject;
                uint32_t _length = sizeof(CommonObject)/sizeof(const PropertySpec_t);
                context_id = r3_property_find(pspec, _length, property_identifier);
            }

            if (context_id==0xFFFF){
                *rsp++ = 0x5E;

                *rsp++ = 0x5F;
            } else {
                void* data = (void*)object + pspec[context_id].offset;
                /// \todo обработка индекса если массив
                *rsp++ = 0x4E;
                 rsp = _pspec_encode_param(rsp, data, context_id, pspec);
                 //rsp = r3_pspec_encode_param (rsp, data, context_id, pspec[context_id].ref);/// \todo контекстный идентификатор откуда берется?
                *rsp++ = 0x4F;
            }
        }
        *rsp++ = 0x1F;
        buf++;//ASSERT(*buf++ == 0x1F);
    }
    return rsp-response_buffer;
}
/*! \brief обработка запроса на изменение свойств объекта

WriteAccessSpecification ::= SEQUENCE {
    object-identifier  [0] BACnetObjectIdentifier,
    list-of-properties [1] SEQUENCE OF BACnetPropertyValue
}
*/
int r3cmd_write_property_multiple(tree_t** device_tree, uint8_t * buffer, size_t length, uint8_t * response_buffer)
{
    uint8_t *buf = buffer;
    while (buf-buffer<length) {
        ASSERT(*buf == 0x0C);
        uint32_t object_identifier = decode_oid(&buf);
        //r3_decode_primitive(buf, &object_identifier, 0, ASN_TYPE_OID);

        Object_t * object = tree_lookup(*device_tree, object_identifier);
        if (object==NULL)
            return ERROR(OBJECT, UNKNOWN_OBJECT);
        const PropertySpec_t * pspec = object->property_list.array;
        uint16_t pspec_length = object->property_list.count;
        if (pspec == NULL) {
            const DeviceObjectClass *object_class = object_classes_lookup(object_identifier>>22);
            if (object_class==NULL)
                return ERROR(OBJECT, UNSUPPORTED_OBJECT_TYPE);
            pspec = object_class->pspec;
            pspec_length = object_class->pspec_length;
        }

        ASSERT(*buf++ == 0x1E);
        while (*buf != 0x1F) {
            ASSERT((*buf & 0xF8) == 0x08);
            uint32_t property_identifier = decode_unsigned(&buf);
            uint16_t context_id = r3_property_find(pspec, pspec_length, property_identifier);
            if ((*buf & 0xF8) == 0x18){// array_index -- if omitted with an array the entire array is referenced
                uint32_t property_array_index =decode_unsigned(&buf);
            }
            void* data = object + pspec[context_id].offset;
            ASSERT(*buf++ == 0x2E);// property-value
            buf = r3_pspec_decode_param(buf, data, /* context_id, */pspec[context_id].ref);
            ASSERT(*buf++ == 0x2F);// priority -- used only when property is commandable
            uint8_t priority = 0;
            if ((*buf & 0xF8) == 0x38) {
                priority = decode_unsigned(&buf);
            }
        }
        /// опционально мы добавляем CRC32 или CMAC
        buf++;// == 0x1F
    }
    return 0;
}
int r3cmd_add_list_element (tree_t** device_tree, uint8_t* buffer, size_t length)
{
    uint8_t *buf = buffer;
    ObjectPropertyReference_t pref;
    ASSERT(*buf == 0x0C);// SD Context Tag 0 (Object Identifier, L=4)
    pref.object_identifier = decode_oid(&buf);
    ASSERT((*buf & 0xF8) == 0x18);//SD Context Tag 1 (Property Identifier, L=1)
    pref.property_identifier = decode_unsigned(&buf);
    if ((*buf & 0xF8) == 0x28) {
        pref.property_array_index = decode_unsigned(&buf);
    }
    int result = r3cmd_property_reference (device_tree, &pref, 0);
    if (result) return result;
// проверить явлется ли листом
    List* list=*(List**)pref.data;
    if (list) while (list->next) list=list->next;

    ASSERT(*buf++ == 0x3E);// PD Opening Tag 3 (List Of Elements)
// проверить явлется ли SEQUENCE OF
    while (*buf != 0x3F) {
        List* elem = g_slice_alloc(pref.pspec->size+sizeof(List*));
        elem->next = NULL;
///        buf = r3_pspec_decode_param(buf, elem->data, pref.pspec);
        if(list) list->next= elem;
        else *(List**)pref.data =elem;
        list=elem;
    }
    ASSERT(*buf++ == 0x3F);
    return 0;// Simple-Ack
}
// сравнение двух элементов списка
static int r3_param_compare(void *a, void* b, const ParamSpec_t* pspec)
{
    int result =0;
    switch (pspec->asn_type&0xF0) {
    default:
        switch(pspec->asn_type&0xF){
        case 1:
            result = *(uint8_t *)a == *(uint8_t *)b;
            break;
        case 2:
            result = *(uint16_t*)a == *(uint16_t*)b;
            break;
        default:
            result = *(uint32_t*)a == *(uint32_t*)b;
            break;
        }
        break;
    }
    return result;
}
int r3cmd_remove_list_element (tree_t** device_tree, uint8_t* buffer, size_t length)
{
    uint8_t *buf = buffer;
    ObjectPropertyReference_t pref;
    ASSERT(*buf == 0x0C);// SD Context Tag 0 (Object Identifier, L=4)
    pref.object_identifier = decode_oid(&buf);
    ASSERT((*buf & 0xF8) == 0x18);//SD Context Tag 1 (Property Identifier, L=1)
    pref.property_identifier = decode_unsigned(&buf);
    if ((*buf & 0xF8) == 0x28) {
        pref.property_array_index = decode_unsigned(&buf);
    } else pref.property_array_index = 0xFFFF;
    int result = r3cmd_property_reference (device_tree, &pref, 0);
    if (result) return result;
// проверить явлется ли листом
    ASSERT(*buf++ == 0x3E);
//    buf = r3_decode_tag(buf, &node_tag, &node_length);
    void *elem_data = malloc(pref.pspec->size);// на стеке?
    while (*buf != 0x3F) {
        //void* elem_data = g_slice_alloc(pspec->size);
///        buf = r3_pspec_decode_param(buf, elem_data, pref.pspec);// исключить копирование строк
        List* list = *(List**)pref.data;
        List* prev = NULL;
        while (list) {
///            if(r3_param_compare(list->data, elem_data, pref.pspec))
            {// если элементы сравнимы, удалить элемент списка
                if(prev) prev->next = list->next;
                else *(List**)pref.data = list->next;
                r3_pspec_free(list->data, pref.pspec->ref, pref.pspec->size);// освободить
                g_slice_free1(pref.pspec->size+sizeof(List), list);
                break;
            }
            prev = list;
            list=list->next;
        }
    }
    buf++;// == 0x3F);
    free(elem_data);
//    buf = r3_decode_tag(buf, &node_tag, &node_length);
    return result;
}
/// TODO добавить readRange и writeGroup
