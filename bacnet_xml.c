#include "bacnet.h"
#include "bacnet_asn.h"
#include "bacnet_csml.h"
#include "r3_slice.h"
#include <stdio.h>
/*!
Consumers are required to:
(a) parse and check the single default namespace specifier "xmlns" specified in Clause Q.2.1.
(b) parse and ignore namespace specifiers it doesn't understand.
(c) parse and ignore elements or attributes with namespace prefixes for namespaces it doesn't understand.
(d) support XML entities "quot", "amp", "apos", "lt", and "gt" (e.g., "&gt;").
(e) parse CDATA sections (e.g., "<![CDATA[...something...]]>"). The CDATA wrapper is not part of the value of
the String (e.g., value above is "...something...").
(f) support numeric character references in the form "&#nnnn;" and "&#xhhhh;", where nnnn is the code point in
decimal and hhhh is the code point in hexadecimal. The 'x' shall be lowercase. The nnnn or hhhh can be any
number of digits and can include leading zeros. The hhhh can mix uppercase and lowercase.
(g) parse all standard elements and attributes defined by this standard.
(h) parse body text for elements that specifically call for it (e.g., <Documentation>, <ErrorText>, etc.)
(i) check valid contents of standard elements and attributes that the implementation supports. Based on the
implementation's capabilities, contents of unsupported items can be ignored.

*/

/*! массив атрибутов  и тегов должен быть отсортирован
Сортировка выполняется в bash с использованием утилиты
# cat table.txt | sort
*/
//#define ATTR(name) {#name, ASN_TYPE_STRING},
#define ATTR(name,t) {#name, t}
struct _namespace_attr {
    const char* name;
    uint8_t asn_type;
};

static const struct _namespace_attr namespace_attrs[] = {
    {NULL, ASN_TYPE_NULL},
    CSML_ATTRS
};
#undef ATTR
#define TAG(name) #name
static const char* namespace_tags[] = {
NULL,
CSML_TAGS
};
#undef TAG

typedef struct _Node Node_t;
typedef struct _Attr Attr_t;
struct _Node {
    Node_t* next;       //!< следующий элемент в списке
//    uint8_t tag;        //!< описание типа???
    uint16_t tag_id;    //!< контекстный идентификатор
    Attr_t* attr;       //!< списко атрибутов
    Node_t* children;   //!< вложенные теги
};
struct _Attr {
    Attr_t* next;       //!< следующий элемент в списке
    BACnetValue value;
};
/*! \brief функция производит сравнение строк,


*/
static inline
int strncmpx(const char *a, const char *b, size_t length)
{
    unsigned char c1;
    unsigned char c2;
	int dif;
	do{
          c1 = (unsigned char) *a++;
          c2 = (unsigned char) *b++;
		if((dif = c1 - c2)) return dif;
	} while (--length);
	return 0-(unsigned char)b[0];// фантомный нолик в конце строки 'а'
}
#if 0
int strncmp (const char *s1, const char *s2, size_t n)
{
    unsigned char c1 = '\0';
    unsigned char c2 = '\0';
    while (n > 0){
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
        break;
      n--;
    }
    return c1 - c2;
}
#endif // 0
/*! \brief бинарный поиск по массиву имен
    \return индекс в массиве или -1
 */
static int _bsearch(const char *key, size_t key_len,  const char**base, size_t num, size_t size)
{
	size_t l = 0, u = num;
//	int count =0; // число шагов
	while (l < u) {
		register const size_t mid = (l + u)>>1;
		register const char* p = *(void**)((uint8_t*)base + mid * size);
		register int result = strncmpx(key, p, key_len);
//		count++;
		if (result < 0)
			u = mid;
		else if (result > 0)
			l = mid + 1;
		else {
//            printf("Step counts = %d \n", count);
			return mid;
		}
	}
	return 0;// не найден
}
static int _xml_tag_id(char* key, size_t key_len)
{
    return _bsearch(key, key_len, namespace_tags, sizeof(namespace_tags)/sizeof(char*), sizeof(char*));
//    return -1;
}
static int _xml_attr_id(char* key, size_t key_len)
{
    return _bsearch(key, key_len, (void*)namespace_attrs, sizeof(namespace_attrs)/sizeof(struct _namespace_attr), sizeof(struct _namespace_attr));
//    return -1;
}
/*! \brief создает новый узел со списокм атрибутов */
static Node_t* _xml_node_new(char* key, char* end)
{
    uint8_t name_id = _xml_tag_id(key, end - key);
    if (0) printf ("tag[%d] = %s\n", name_id, namespace_tags[name_id]);
    Node_t* node = g_slice_alloc(sizeof(Node_t));
    node->tag_id = name_id;
    node->attr = NULL;// список атрибутов
    node->children = NULL;
    node->next = NULL;
    return node;
}
/*! \brief добавить список узлов в конец списка дочек */
static void _xml_node_append(Node_t* parent, Node_t* node)
{
   Node_t *child = parent->children;
    if (child) {
        while (child->next) child = child->next;
        child->next = node;
    } else
        parent->children = node;
}
/*! \brief выбор атрибута */
BACnetValue* csml_attr(Node_t* node, uint32_t attr_id)
{
    Attr_t* attr = node->attr;
    while (attr){
        if (attr->value.type_id == attr_id) {
            return &attr->value;
        }
        attr = attr->next;
    }
    return NULL;
}
/*! \brief
    \param path список идентификаторов тегов, заканчивается 0.
*/
Node_t * csml_path(Node_t* node, const uint8_t *path)
{
    uint8_t tag_id;
    while (node!=NULL && (tag_id = *path++)!=0) {
        node = node->children;
        while (node) {
            if (node->tag_id == tag_id) {
                break;
            }
            node = node->next;
        }
    }
    return node;
}


/*! \brief создает текст */
static void _xml_text_set(Node_t* node, char* str, uint16_t length)
{
    Attr_t* attr = g_slice_alloc(sizeof(Attr_t));
    attr->value.tag = ASN_TYPE_STRING;// строка выделена статически!!!
    attr->value.type_id = 0;
    attr->value.length  = length;
    attr->value.value.s = str;

    attr->next = node->attr;// добавить в список атрибутов
    node->attr = attr;
}
/*! \brief создает новый атрибут */
static void _xml_attr_new(Node_t* node, uint8_t attr_id, char* str, uint16_t str_len)
{
    Attr_t* attr = g_slice_alloc(sizeof(Attr_t));
    attr->value.tag = ASN_TYPE_STRING|ASN_CLASS_CONTEXT;// указывает на наличие поля - идентификатор атрибута
    attr->value.type_id = attr_id;
    attr->value.length  = str_len;
    attr->value.value.s = str;

    attr->next = node->attr;// добавить в список атрибутов
    node->attr = attr;
}

#define XER_SYM_MAKE(d,c,b,a)   ((uint32_t)(a)+((uint32_t)(b)<<8)+((uint32_t)(c)<<16)+((uint32_t)(d)<<24))

#define XER_XML_COMMENT_OPEN_SYMBOL		XER_SYM_MAKE(0, 0, '<', '!')
#define XER_XML_COMMENT_OPEN_MASK		XER_SYM_MAKE(0, 0,0xFF,0xFF)

#define XER_XML_COMMENT_CLOSE_SYMBOL	XER_SYM_MAKE(0, '-', '-', '>')
#define XER_XML_COMMENT_CLOSE_MASK		XER_SYM_MAKE(0,0xFF,0xFF,0xFF)

#define XER_XML_TAG_OPEN_SYMBOL			XER_SYM_MAKE(0, 0, '<', 0x00)
#define XER_XML_TAG_OPEN_MASK			XER_SYM_MAKE(0, 0,0xFF, 0x00)

#define XER_XML_TAG_CLOSE_SYMBOL		XER_SYM_MAKE(0, 0, '<', '/')
#define XER_XML_TAG_CLOSE_MASK			XER_SYM_MAKE(0, 0,0xFF,0xFF)
/*! внутри символьных данных <![CDATA[ ... ]]> разметка игнорируется
    DTD может включать теги
    <!ATTLIST  ...  >
    <!ENTRY    ...  >
    <!NOTATION ...  >
    <!ELEMENT  ...  >
 */
// <!-- ... -->
#define XER_XML_COMMENT_SYMBOL		    XER_SYM_MAKE( '<', '!', '-', '-')
#define XER_XML_COMMENT_MASK			XER_SYM_MAKE(0xFF,0xFF,0xFF,0xFF)
// <![ CDATA [ ... ]]>
#define XER_XML_DTD_SYMBOL		        XER_SYM_MAKE(0,    '<', '!', '[')
#define XER_XML_DTD_MASK			    XER_SYM_MAKE(0,   0xFF,0xFF,0xFF)

#define XER_XML_DOCTYPE_SYMBOL		    XER_SYM_MAKE( '<', '!', 0,0)
#define XER_XML_DOCTYPE_MASK			XER_SYM_MAKE(0xFF,0xFF, 0,0)

#define XER_XML_DTD_CLOSE_SYMBOL		XER_SYM_MAKE(0, 0, ']', '>')
#define XER_XML_DTD_CLOSE_MASK		    XER_SYM_MAKE(0, 0,0xFF,0xFF)

#define XER_XML_CDATA_CLOSE_SYMBOL		XER_SYM_MAKE(0, ']', ']','>')
#define XER_XML_CDATA_CLOSE_MASK		XER_SYM_MAKE(0, 0xFF,0xFF,0xFF)

#define XER_XML_EXT_OPEN_SYMBOL			XER_SYM_MAKE(0, 0, '<', '?')
#define XER_XML_EXT_OPEN_MASK			XER_SYM_MAKE(0, 0,0xFF,0xFF)

#define XER_XML_EXT_CLOSE_SYMBOL		XER_SYM_MAKE(0, 0, '?', '>')
#define XER_XML_EXT_CLOSE_MASK			XER_SYM_MAKE(0, 0,0xFF,0xFF)

#define IS_NAME_TAIL(c) (((c)>='a' && (c)<='z') || ((c)>='A' && (c)<='Z'))//(IS_ALPHA(c) || IS_NUMBER(c) || (c)=='-' || (c)=='_')
#define IS_SPACE(c) ((c)==' ' || (c)=='\t' || (c)=='\n' || (c)=='\r')
#define IS_ALPHA(c) (((c)>='A' && (c)<='Z') || ((c)>='a' && (c)<='z'))
#define IS_STRING(c) ((c)== '"')
#define IS_NUMBER(c) ((c)>='0' && (c)<='9')

Node_t* csml_parse(char *buf, size_t length)
{
    uint32_t symbol=0;
    uint8_t  c;
    enum {
        XML_ST_DEFAULT, XML_ST_TAG_NAME, XML_ST_ATTR_NAME, XML_ST_TAG,
        XML_ST_DTD, XML_ST_MULTI_COMMENT, XML_ST_EXTENTION,
        XML_ST_VALUE,XML_ST_OPERATOR,XML_ST_COMMENT,
        XML_ST_CLOSE_TAG,XML_ST_ATTR_VALUE,XML_ST_CLOSE_TAG_NAME
    } state = XML_ST_DEFAULT;
    char *str = buf;
    char *end = buf+length;
    Node_t* root = NULL;
    Node_t* node = NULL;
    Node_t* parents[16];// = NULL;
    int depth=0;
    uint16_t attr_id;

    while (buf<end)
    {
        c = *buf++;
        symbol = (symbol<<8) | c;

        switch (state){
		case XML_ST_TAG_NAME:{// выделяем имя тега
            if (IS_NAME_TAIL(c)){
                break;
            } else {
                Node_t* new_node = _xml_node_new(str, buf-1);
                //new_node->next = node;
#if 1
                if (node == NULL) {
                    if (depth)
                        parents[depth-1]->children = new_node;
                    else
                        root = new_node;
                } else
                    node->next = new_node;
#endif // 0
                node = new_node;

                state = XML_ST_TAG; // внутри тега

            }
        } // проходим дальше
		case XML_ST_TAG: // обработка внутри тега
            if (IS_SPACE(c)){// пропустить пробелы
            } else
            if (IS_ALPHA(c)){
                str = buf-1;
                state = XML_ST_ATTR_NAME;
            } else
            if (c == '/'){// short tag />
                state = XML_ST_CLOSE_TAG;
			} else
			if (c == '>'){ // закрытие тега
                if (0) printf (" -- step inside %s\n", namespace_tags[node->tag_id]);
			    parents[depth++] = node; node = NULL;
                state = XML_ST_VALUE;
                str = buf;
            } else {
                /// error форматирование тега.
            }
            break;
        case XML_ST_VALUE: {
            if (IS_SPACE(c)){
                str = buf;
            } else
            if (c == '<') {
                if (buf-str-1 > 0)
                    _xml_text_set(node, str, buf-str-1);
                state = XML_ST_OPERATOR;
            }
        } break;
        case XML_ST_ATTR_NAME:
            if (IS_NAME_TAIL(c)) {
            } else {
                attr_id = _xml_attr_id(str, buf-str-1);
                if (0) printf ("  attr[%d] = %s\n", attr_id, namespace_attrs[attr_id].name);

                if (c == '=') {
                    state = XML_ST_ATTR_VALUE;
                } else {// error
                }
            }
            break;
        case XML_ST_ATTR_VALUE:
            if (c == '"') {
                str = buf;
                while ((*buf++) != '"');
                if (0) printf ("  attr[%d]: %s=\"%.*s\"\n", attr_id, namespace_attrs[attr_id].name, buf-str-1, str);
                _xml_attr_new(node, attr_id, str, buf-str-1);
                state = XML_ST_TAG;// внутри тега
            }
            break;
        case XML_ST_DTD:
            if ((symbol & XER_XML_DTD_CLOSE_MASK) == XER_XML_DTD_CLOSE_SYMBOL) {
                state = XML_ST_DEFAULT;// просто выкинуть комент
                str = buf;
            }
            break;
        case XML_ST_MULTI_COMMENT: // многострочный коммент
            if (c == '-') {//
                state = XML_ST_COMMENT;
            } else
            if (c == '[') {// <![CDATA[.....]]>
                state = XML_ST_DTD;
            }
            break;
        case XML_ST_COMMENT:
            if ((symbol & XER_XML_COMMENT_CLOSE_MASK) == XER_XML_COMMENT_CLOSE_SYMBOL) {// -->
                state = XML_ST_DEFAULT;// просто выкинуть комент
                str = buf;
            }
            break;
        case XML_ST_EXTENTION: // тег php, xml или пр выделяем до ?>
            if ((symbol & XER_XML_EXT_CLOSE_MASK)==XER_XML_EXT_CLOSE_SYMBOL) {
                state = XML_ST_DEFAULT;
                str = buf;
			}
            break;
        case XML_ST_DEFAULT:
			if (c == '<') {/// начало нового тега или закрывающий тег
				state = XML_ST_OPERATOR;
			}
			break;
        case XML_ST_OPERATOR: // проверяю все операторы, которые начинаются с '<'
            if (IS_ALPHA(c)) {// открывающий тег
            	state = XML_ST_TAG_NAME;
            	str = buf-1;// возвращаем один символ
			} else
			if ( c == '/') {// закрывающий тег --
            	state = XML_ST_CLOSE_TAG_NAME;
                str = buf;
			} else
            if (c == '!') { // <!
            	state = XML_ST_MULTI_COMMENT;
            	str = buf-2;
			} else
			if (c == '?') { // специальные комменты типа <?php  ?> <?xml ?>
            	state = XML_ST_EXTENTION;
            	str = buf-2;
			} else {
			    /// сообщить об ошибке
			}
            break;
        case XML_ST_CLOSE_TAG_NAME:
            if (IS_NAME_TAIL(c)) {
            } else {
                int tag_id = _xml_tag_id(str, buf-str-1);

                node = parents[--depth];
                if (0) printf(" -- step outside %s\n", namespace_tags[tag_id]);
                if (0) printf(" close tag[%d] =/%s\n", tag_id, namespace_tags[tag_id]);

                ///  в этой точке можно проверить соответствие имен
                if (c=='>'){
                    state = XML_ST_DEFAULT;
                } else {
                    state = XML_ST_CLOSE_TAG;
                }
            }
            break;
		case XML_ST_CLOSE_TAG: // обработка внутри тега
		    if (c == '>') { // close tag
                state = XML_ST_DEFAULT;
		    }
		    break;
        }
    }
    return root;
}
/*! \brief освободить ресурсы занятые под хранение дерева и атрибутов */
void csml_free(Node_t* node)
{
    while (node) {
        Node_t* node_next = node->next;
        csml_free(node->children);
        Attr_t* attr = node->attr;
        while (attr){
            Attr_t* attr_next = attr->next;
            g_slice_free1(sizeof(Attr_t), attr);
            attr = attr_next;
        }
        g_slice_free1(sizeof(Node_t), node);
        node = node_next;
    }
}
void csml_debug(Node_t* node)
{
    static int offset = 0;
    while (node) {
        printf ("%*s<%s", offset,"", namespace_tags[node->tag_id]);
        Attr_t* attr = node->attr;
        while (attr){
            printf(" %s=\"%.*s\"", namespace_attrs[attr->value.type_id].name, attr->value.length, attr->value.value.s);
            attr = attr->next;
        }
        if (node->children) {
            printf (">\n");// todo текст вписать
            offset+=2;
            csml_debug(node->children);
            offset-=2;
            printf ("%*s</%s>\n", offset,"", namespace_tags[node->tag_id]);
        } else {// short tag
            printf ("/>\n");
        }
        node = node->next;
    }
}

#ifdef TEST_XML
#include "r3_string.h"
/*! \brief преобразование формата CSML в JSON
    \see ANSI/ASHRAE Std 135-2016 ANNEX Z - JSON DATA FORMATS
    \see [RFC 8259] The JavaScript Object Notation (JSON) Data Interchange Format, December 2017

    В целом я не хочу поддерживать формат JSON потому что он кривой! Ожидаемой экономии не получается

    <Sequence>
        <Unsigned value="1"/>
    </Sequence>
    может быть представлено коротко так:
    {"Sequence",
        {"Unsigned", "$value":"1"}
    }
    -- это выглядит короче, но реально стандарт требует такого предствления:
    "myvalue":{"$base":"Sequence",
        "1":{"$base":"Unsigned", "$value":"1"}
    }
    -- выигрыш не виден, формат менее читабельный


*/
int csml_json(Node_t* node, String_t *str)
{
    while (node) {
        uint16_t tag_id = node->tag_id;
        BACnetValue *name = csml_attr(node, CSML_ATTR_name);
        if (name) {
            g_string_append(str, "\"");
            g_string_append_len(str, name->value.s, name->length);
            g_string_append(str, "\":{");
        } else {
            g_string_append(str, "{");// \"1\"
        }
//        g_string_append(str, "\"$base\":");
        g_string_append(str, "\"");
        g_string_append(str, namespace_tags[tag_id]);
        g_string_append(str, "\",");

        Attr_t* attr = node->attr;
        while (attr) {
            BACnetValue *attr_val = &attr->value;
            if (attr_val->type_id!=CSML_ATTR_name) {

            g_string_append(str, "\"$");
            g_string_append(str, namespace_attrs[attr_val->type_id].name);
            g_string_append(str, "\":");

            //while (attr) {
#if 1
            if (attr_val->tag & ASN_CLASS_CONSTRUCTIVE) {
                g_string_append_c(str, '{');

                g_string_append_c(str, '}');
            } else
            switch (attr_val->tag & 0xF0){
        //            case ASN_TYPE_ARRAY:
            case ASN_TYPE_NULL:
                g_string_append_len(str, "null", 4);
                break;
            case ASN_TYPE_BOOLEAN:
                g_string_append(str, attr_val->value.b?"true":"false");
                break;
            case ASN_TYPE_STRING:
                g_string_append_c(str, '"');
                /// может понадобится кодирование см g_string_append_escaped
                g_string_append_len(str, attr_val->value.s, attr_val->length);
                g_string_append_c(str, '"');
                break;
            case ASN_TYPE_OCTETS:// xs:hexBinary
                g_string_append_hex(str, attr_val->value.octets, attr_val->length);
                break;
            case ASN_TYPE_BIT_STRING: //
            case ASN_TYPE_DOUBLE: //
            case ASN_TYPE_REAL: //
            case ASN_TYPE_DATE: //
            case ASN_TYPE_TIME: //
            case ASN_TYPE_UNSIGNED:
            case ASN_TYPE_ENUMERATED:
            default:
                break;
            }
#endif
            g_string_append(str, ",");
        }
            attr = attr->next;
        }

        if (node->children){
            g_string_append(str, "\n");
            csml_json(node->children, str);
//            g_string_append(str, "},\n");
        }
        g_string_append(str, "},\n");
        node = node->next;
    }
    return 0;
}

// пробуем! создать устройтсво по описанию csml/ бывает JSON бывает ASN.
/*! \brief
 */
Node_t* csml_get_object(Node_t* root, BACnetObjectIdentifier* oid, BACnetValue* value);

uint8_t* csml_attr_store(Attr_t* attr, uint8_t * buffer, size_t len)
{
    uint8_t * buf = buffer;
    while(attr){
        buf = bacnet_value_encode(&attr->value, buf, len - (buf-buffer));
        attr = attr->next;
    }
    return buf;
}
/*! \brief сохранение происходит в бинарном виде
    этот формат мы хотим использовать для device_tree
    может быть на приватный транспрт такое кодирование сделать?
*/
uint8_t* csml_node_header(Node_t* root, uint8_t * buffer, size_t len)
{
    // команда
    транзакция!
}
uint8_t* csml_node_store(Node_t* root, uint8_t * buffer, size_t len)
{
    uint8_t * buf = buffer;
    Node_t* node = root;
    while (node) {
        *buf++ = 0x0A;// тег котекстный,длина два байта
        *buf++ = node->tag_id>>8;
        *buf++ = node->tag_id;
        if (node->attr) {
            *buf++ = 0x1E;// начало списка атрибутов
            buf = csml_attr_store(node->attr, buf, len - (buf-buffer));
            *buf++ = 0x1F;// конец списка атрибутов
        }
        if (node->children){
            *buf++ = 0x2E;// начало списка дочерних объектов
            buf = csml_node_store(node->children, buf, len - (buf-buffer));
            *buf++ = 0x2F;// конец списка дочек
        }
        if (crc){
            /// кодировать CRC32
        }
        node=node->next;
    }
    return buf;
}
uint8_t* csml_attr_load(Node_t** root, uint8_t * buffer, size_t len)
{
    uint8_t *buf = buffer;
    do {
// .
    } while ((*buf & ASN_CLASS_CLOSING)!=ASN_CLASS_CLOSING);
    return buf;
}
uint8_t* csml_node_load(Node_t** root, uint8_t * buffer, size_t len)
{
    uint8_t * buf = buffer;
    Node_t* node = NULL;
    while (buf<buffer+len) {
        if(*buf!=0x0A) {
            //
            break;
        }
        buf++;
        Node_t* n = g_slice_alloc(sizeof(Node_t));
        n->next = NULL, n->attr=NULL, n->children=NULL;
        if (node) node->next = n;
        else *root = n;
        node = n;

        node->tag_id = (buf[0]<<8) | buf[1]; buf++;
        if (*buf==0x1E) {// начало списка атрибутов
            buf++;
            buf = csml_attr_load(&node->attr, buf, len - (buf-buffer));
            buf++;// конец списка атрибутов, должно быть 0x1F
        }
        if (*buf==0x2E){// начало списка дочерних объектов
            buf++;
            buf = csml_node_load(&node->children, buf, len - (buf-buffer));
            buf++;// конец списка дочек, должно быть 0x2F
        }
    }
    return buf;
}

int service_run=false;
#include <stdio.h>
#include <string.h>
int main()
{
    int tag_id;
//    tag_id = _xml_tag_id("String",6);
    tag_id = _xml_tag_id("Date",4);
    printf(" Test tag=%d '%s'\n", tag_id, namespace_tags[tag_id]);

char test1[] = "<Definitions>"
"<Sequence name=\"0-BACnetPropertyReference\">"
"<Enumerated name=\"property-dentifier\" contextTag=\"0\" type=\"0-BACnetPropertyIdentifier\" />"
"<Unsigned name=\"property-array-index\" contextTag=\"1\" optional=\"true\""
"    comment=\"Used only with array datatype. If omitted, the entire array is referenced.\"></Unsigned>"
"</Sequence>"
"</Definitions>";

char test2[] =
"<Sequence type=\"0-BACnetDailySchedule\">"
"<SequenceOf name=\"day-schedule\">"
"<Sequence>"
"<Time name=\"time\" value=\"08:00:00.00\"/>"
"<Unsigned name=\"value\" value=\"1\"/>"
"</Sequence>"
"<Sequence>"
"<Time name=\"time\" value=\"15:00:00.00\"/>"
"<Unsigned name=\"value\" value=\"0\"/>"
"</Sequence>"
"</SequenceOf>"
"</Sequence>";
    Node_t *node = csml_parse(test2, strlen(test2));
    csml_debug(node);
    String_t str1;
    char buffer[512];

    String_t* str = g_string_init(&str1, buffer, 512);
    csml_json(node, str);
    printf("JSON (%d):\n%*s\n", str->len,str->len, str->str);
    csml_free(node);

    return 0;
}
#endif // TEST_XML
