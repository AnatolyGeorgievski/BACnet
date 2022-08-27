/*! \brief */

#include <stddef.h>
#include <stdint.h>
#include "bacnet.h"
#include "bacnet_asn.h"
#include "bacnet_object.h"
#include "r3_slice.h"

#pragma GCC optimize ("O2")

BACnetValue * bacnet_value_list_new(uint8_t type_id)
{
	BACnetValue * node = g_slice_new(BACnetValue);
	node->tag = ASN_CLASS_CONSTRUCTIVE|ASN_CLASS_CONTEXT;
	node->type_id = type_id;
	node->value.list =NULL;
	return node;
}
BACnetLIST * bacnet_value_list_append(BACnetLIST * top, BACnetValue* node)
{
    BACnetLIST * last = g_slice_new(BACnetLIST);
    last->value.node = node;
    last->next = NULL;
    if (top!=NULL) {
        BACnetLIST * list = top;
        while (list->next) list = list->next;
        list->next = last;
    } else {
        top = last;
    }
    return top;
}


static inline uint8_t * bacnet_encode_constructive (uint8_t *buf, uint8_t type_id, uint8_t class_id)
{
    class_id &= 0xF;
	if (type_id >= 0xF) {
        *buf++ = class_id | 0xF0;
        *buf++ = type_id;
	} else {
	    *buf++ = class_id | (type_id<<4);
	}
	return buf;
}

/*! \brief преобразует дерево в представлении BACnetNode в формат bacnet
    \todo для тестирования надо сделать вывод в формате .pcap для wireshark
    \todo проверить чтобы не превышать размера буфера size.

    0..2 - длина, 5- удлинение поля типа, 6- начало, 7- конец конструктивного типа
    3    - идентификатор контекстного типа
    4..7 - идентификатор типа или номер контекстного типа
 */
uint8_t* bacnet_value_encode(BACnetValue* node, uint8_t* buf, size_t size)
{
    uint16_t length;
    //1 корректировка поля длины
    switch (node->tag & 0xF0) {// // 1. старшие четыре бита node->tag обязаны содержать указание на основной тип данных
    case ASN_TYPE_BOOLEAN:
        length = ((node->tag&ASN_CLASS_CONTEXT) || node->value.b)? 1: 0;
        break;
    case ASN_TYPE_INTEGER:
        if (node->value.i<0) {
            length = ~node->value.u!=0? 4-((__builtin_clz(~node->value.u)-1)>>3) : 1;
        } else {
            length = node->value.u!=0? 4-((__builtin_clz(node->value.u)-1)>>3) : 0;
        }
        break;
    case ASN_TYPE_REAL:
        length = 4;
        break;
    default:/// \todo дописать
    case ASN_TYPE_ENUMERATED:
    case ASN_TYPE_UNSIGNED:
        if (node->value.u!=0) {
            length = 4-(__builtin_clz(node->value.u)>>3);
        } else {
            length = 0;
        }
        break;
    }
//2 кодирование тега и длины
    buf = bacnet_encode_tag(buf, node, length);
    if (length>4){
        __builtin_memcpy(buf, node->value.octets, length);
        buf+=length;
    } else {
        if ((node->tag & 0xF8)== ASN_TYPE_BOOLEAN){
        } else {
            switch (length) {
            case 4: *buf++ = node->value.u>>24;
            case 3: *buf++ = node->value.u>>16;
            case 2: *buf++ = node->value.u>>8;
            case 1: *buf++ = node->value.u;
            default: break;
            }
        }
    }
    return buf;
}

size_t bacnet_paramspec_encode(uint8_t* buf, void* obj, const ParamSpec_t* pspec, int size)
{
	uint8_t* ref = buf;
	BACnetValue node={.type_id=0};

    int i;
    for (i=0; i<size; i++) { // не конструктивно
        node.type_id    = pspec[i].asn_type>>4;
        node.length = pspec[i].size;
        if (node.length==1){
            node.value.u = *(uint8_t *)(obj + pspec[i].offset);
        } else
        if (node.length==2){
            node.value.u = *(uint16_t*)(obj + pspec[i].offset);
        }
        else {
            node.value.u = *(uint32_t*)(obj + pspec[i].offset);
        }
        buf = bacnet_value_encode(&node, buf, -1);
    }
    return buf-ref;
}
/*! \brief разбирает формат bacnet и дерево в представлении BACnetNode
 */
uint8_t* bacnet_node_decode(uint8_t* buf, BACnetValue* parent, /* const BacContext* tpl, */uint32_t size)
{
//    BACnetNode* node = g_slice_new(BACnetNode);// из множества объектов одного размера
//    uint8_t context_id = 0;
    BACnetLIST* list = parent->value.list;
while(size>0 && (buf[0] & ASN_CLASS_MASK)!=ASN_CLASS_CLOSING) {
    BACnetValue* node = g_slice_new(BACnetValue);
    list = bacnet_value_list_append(list, node);
    uint8_t* data = buf;
    buf = bacnet_decode_tag(buf, node);


    if ((node->tag & ASN_CLASS_MASK) == ASN_CLASS_OPENING) {
        node->value.list=NULL;
        buf = bacnet_node_decode(buf, node, /* NULL, */size - (buf-data));
        if (buf[0] == 0xFF) {
            buf++;
        }
        buf++;
    } else { // не конструктивно
        if (node->length > 4){
            node->value.octets = buf;
        } else {
                                //context[node->type_id] : node->tag;
            //if (node->tag&)
            switch (node->tag&0xF8) {// контекстные игнорим
            case ASN_TYPE_BOOLEAN:
                node->value.b =node->length;
                node->length =0;
                break;
            case ASN_TYPE_STRING:
                node->value.s =(char*)buf;
                break;
            default: {
                uint32_t value = 0;
                int i;
                for (i=0; i<node->length; i++){
                    value = (value<<8) | buf[i];
                }
                node->value.u = value;
              } break;
            }
        }
        buf+=node->length;
    }
    size -= buf - data;
}
parent->value.list = list;
    return buf;
}

void bacnet_value_unset(BACnetValue* node)
{
    if ((node->tag & ASN_CLASS_MASK) == ASN_CLASS_CONSTRUCTIVE) {
        BACnetLIST* list = node->value.list;
        node->value.list = NULL;
        while (list){
            bacnet_value_free(list->value.node);
            BACnetLIST* next = list->next;
            g_slice_free(BACnetLIST, list);
            list = next;
        }
    }
    *node = (BACnetValue){0};
}
/*! \brief освободить структуру дерева */
void bacnet_value_free(BACnetValue* node)
{
    if ((node->tag & ASN_CLASS_MASK) == ASN_CLASS_CONSTRUCTIVE) {
        BACnetLIST* list = node->value.list;
        node->value.list = NULL;
        while (list){
            bacnet_value_free(list->value.node);
            BACnetLIST* next = list->next;
            g_slice_free(BACnetLIST, list);
            list = next;
        }
    } else {// не конструктивно
        if (node->length > 4){
#if 0
            // строка если копировалась, надо освободить
            g_free(node->value.octets);
#endif
        }
    }
    g_slice_free(BACnetValue, node);
}
