#ifndef BACNET_CSML_H
#define BACNET_CSML_H
#include <stddef.h>
//#include "bacnet_asn.h"
/*! \defgroup _bacnet_csml
 */
#define CSML_TAGS \
TAG(Any),\
TAG(Array),\
TAG(Bit),\
TAG(BitString),\
TAG(Boolean),\
TAG(Choice),\
TAG(Choices),\
TAG(Collection),\
TAG(Comment),\
TAG(Composition),\
TAG(Date),\
TAG(DatePattern),\
TAG(DateTime),\
TAG(DateTimePattern),\
TAG(Definitions),\
TAG(Description),\
TAG(DisplayName),\
TAG(DisplayNameForWriting),\
TAG(Documentation),\
TAG(Double),\
TAG(Enumerated),\
TAG(ErrorText),\
TAG(Extensions),\
TAG(Failures),\
TAG(Includes),\
TAG(Integer),\
TAG(Link),\
TAG(Links),\
TAG(List),\
TAG(MemberTypeDefinition),\
TAG(NamedBits),\
TAG(NamedValues),\
TAG(Null),\
TAG(Object),\
TAG(ObjectIdentifier),\
TAG(ObjectIdentifierPattern),\
TAG(OctetString),\
TAG(PriorityArray),\
TAG(Raw),\
TAG(Real),\
TAG(RelinquishDefault),\
TAG(RequiredWhenText),\
TAG(Revisions),\
TAG(Sequence),\
TAG(SequenceOf),\
TAG(String),\
TAG(StringSet),\
TAG(TagDefinitions),\
TAG(Time),\
TAG(TimePattern),\
TAG(UnitsText),\
TAG(Unknown),\
TAG(Unsigned),\
TAG(Value),\
TAG(ValueTags),\
TAG(WeekNDay),\
TAG(WritableWhenText),

#define CSML_ATTRS \
ATTR(absent,ASN_TYPE_BOOLEAN),\
ATTR(associatedWith,ASN_TYPE_STRING),\
ATTR(authRead,ASN_TYPE_STRING),\
ATTR(authVisible,ASN_TYPE_BOOLEAN),\
ATTR(authWrite,ASN_TYPE_STRING),\
ATTR(base,ASN_TYPE_ENUMERATED),\
ATTR(bit,ASN_TYPE_UNSIGNED),\
ATTR(children,ASN_TYPE_STRING),/* StringSet */\
ATTR(choices,ASN_TYPE_STRING),\
ATTR(commandable,ASN_TYPE_BOOLEAN),\
ATTR(comment,ASN_TYPE_STRING),\
ATTR(contextTag,ASN_TYPE_UNSIGNED),\
ATTR(count,ASN_TYPE_UNSIGNED),\
ATTR(defaultLocale,ASN_TYPE_STRING),\
ATTR(descendants,ASN_TYPE_STRING),/* List of Link */\
ATTR(description,ASN_TYPE_STRING),\
ATTR(displayName,ASN_TYPE_STRING),\
ATTR(displayNameForWriting,ASN_TYPE_STRING),\
ATTR(documentation,ASN_TYPE_STRING),\
ATTR(error,ASN_TYPE_UNSIGNED),\
ATTR(errorText,ASN_TYPE_STRING),\
ATTR(etag,ASN_TYPE_STRING),\
ATTR(extends,ASN_TYPE_STRING),\
ATTR(failures,ASN_TYPE_STRING),/* List of Link */\
ATTR(fault,ASN_TYPE_BOOLEAN),\
ATTR(history,ASN_TYPE_STRING),/* List of Sequence */\
ATTR(href,ASN_TYPE_STRING),\
ATTR(id,ASN_TYPE_STRING),\
ATTR(inAlarm,ASN_TYPE_BOOLEAN),\
ATTR(isMultiLine,ASN_TYPE_BOOLEAN),\
ATTR(length,ASN_TYPE_UNSIGNED),\
ATTR(links,ASN_TYPE_STRING),/* Collection of Link */\
ATTR(locale,ASN_TYPE_STRING),\
ATTR(maximum,ASN_TYPE_STRING),\
ATTR(maximumForWriting,ASN_TYPE_STRING),\
ATTR(mediaType,ASN_TYPE_STRING),\
ATTR(memberType,ASN_TYPE_STRING),\
ATTR(memberTypeDefinition,ASN_TYPE_STRING),\
ATTR(minimum,ASN_TYPE_STRING),\
ATTR(minimumForWriting,ASN_TYPE_STRING),\
ATTR(name,ASN_TYPE_STRING),\
ATTR(namedBits,ASN_TYPE_STRING),\
ATTR(namedValues,ASN_TYPE_STRING),\
ATTR(nodeSubtype,ASN_TYPE_STRING),\
ATTR(nodeType,ASN_TYPE_ENUMERATED),\
ATTR(notForReading,ASN_TYPE_BOOLEAN),\
ATTR(notForWriting,ASN_TYPE_BOOLEAN),\
ATTR(notPresentWith,ASN_TYPE_STRING),\
ATTR(objectType,ASN_TYPE_STRING),\
ATTR(optional,ASN_TYPE_BOOLEAN),\
ATTR(outOfService,ASN_TYPE_BOOLEAN),\
ATTR(overlays,ASN_TYPE_STRING),\
ATTR(overridden,ASN_TYPE_BOOLEAN),\
ATTR(priorityArray,ASN_TYPE_STRING),/* Array of Choice */\
ATTR(propertyIdentifier,ASN_TYPE_UNSIGNED),\
ATTR(readable,ASN_TYPE_BOOLEAN),\
ATTR(relationship,ASN_TYPE_ENUMERATED),\
ATTR(relinquishDefault,ASN_TYPE_STRING),\
ATTR(requiredWhen,ASN_TYPE_STRING),\
ATTR(requiredWhenText,ASN_TYPE_STRING),\
ATTR(requiredWith,ASN_TYPE_STRING), /* StringSet */\
ATTR(requiredWithout,ASN_TYPE_STRING),/* StringSet */\
ATTR(resolution,ASN_TYPE_STRING),\
ATTR(revisions,ASN_TYPE_STRING),\
ATTR(sourceId,ASN_TYPE_STRING),\
ATTR(tags,ASN_TYPE_STRING),/* StringSet */\
ATTR(target,ASN_TYPE_STRING),\
ATTR(targetType,ASN_TYPE_STRING),\
ATTR(type,ASN_TYPE_STRING),\
ATTR(units,ASN_TYPE_ENUMERATED),\
ATTR(unitsText,ASN_TYPE_STRING),\
ATTR(unspecifiedValue,ASN_TYPE_BOOLEAN),\
ATTR(value,ASN_TYPE_STRING),\
ATTR(valueTags,ASN_TYPE_STRING),/* List of Any */\
ATTR(variability,ASN_TYPE_ENUMERATED),\
ATTR(virtual,ASN_TYPE_BOOLEAN),\
ATTR(volatility,ASN_TYPE_ENUMERATED),\
ATTR(writable,ASN_TYPE_BOOLEAN),\
ATTR(writableWhen,ASN_TYPE_STRING),\
ATTR(writableWhenText,ASN_TYPE_STRING),\
ATTR(writeEffective,ASN_TYPE_ENUMERATED), \
ATTR(xmlns,ASN_TYPE_STRING), \

#define TAG(name) CSML_TAG_##name
enum _CSML_TAGS {// перечисление атрибутов,
    _CSML_TAGS_NULL =0,// этот не используется
    CSML_TAGS
    CSML_TAGS_COUNT
};
#undef TAG

#define ATTR(name,t) CSML_ATTR_##name
enum _CSML_ATTRS {// перечисление атрибутов,
    _CSML_ATTR_NULL =0,// этот не используется
    CSML_ATTRS
    CSML_ATTRS_COUNT
};
#undef ATTR
typedef struct _Node Node_t;

Node_t* csml_parse(char *buf, size_t length);
void    csml_debug(Node_t* node);
void    csml_free(Node_t* node);
BACnetValue* csml_attr(Node_t* node, uint32_t attr_id);
#endif // BACNET_CSML_H
