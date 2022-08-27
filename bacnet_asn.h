#ifndef BACnetER_H
#define BACnetER_H
#define ASN_CLASS_APPLICATION    0x0
#define ASN_CLASS_CONTEXT        0x8
#define ASN_CLASS_CONSTRUCTIVE   0x6
#define ASN_CLASS_OPENING        0x6
#define ASN_CLASS_CLOSING        0x7
#define ASN_CLASS_MASK           0x7

#define ASN_TYPE_NULL	         0x00
#define ASN_TYPE_BOOLEAN         0x10
#define ASN_TYPE_UNSIGNED        0x20
#define ASN_TYPE_INTEGER         0x30
#define ASN_TYPE_REAL	         0x40
#define ASN_TYPE_DOUBLE	         0x50
#define ASN_TYPE_OCTETS    		 0x60
#define ASN_TYPE_STRING    		 0x70
#define ASN_TYPE_BIT_STRING		 0x80
#define ASN_TYPE_ENUMERATED		 0x90
#define ASN_TYPE_DATE			 0xA0
#define ASN_TYPE_TIME			 0xB0
#define ASN_TYPE_OID             0xC0

//#define ASN_CONTEXT(n)      (((n)<<4)|ASN_CLASS_CONTEXT)

#include <stdint.h>

typedef struct _BacTemplate BacTemplate;
struct _BacTemplate {
    const char *name;
    unsigned int type;
    const BacTemplate* ref;
    unsigned int flags;
//    unsigned short offset;
//    unsigned short size;
//    const void* default_value;
};

/*! \brief Упаковка базовых типов
    \param type_id - тип, значение типа меньше <= 0xD
    \param class_id - контекстно зависимый класс, 0x8 или 0x0
    \param
 */
#endif // BACnetER_H
