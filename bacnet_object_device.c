/*! */
#include <stdlib.h>
#include "r3_asn.h"
#include "r3_object.h"
#include "bacnet.h"
#include "bacnet_net.h"
const PropertySpec_t CommonObject[] = {
    {OBJECT_IDENTIFIER,     ASN_TYPE_OID,       OFFSET(Object_t, object_identifier)},
    {OBJECT_NAME,           ASN_TYPE_STRING,    OFFSET(Object_t, object_name)},
    {~0}
};
static const PropertySpec_t DeviceObject[] = {
	
    {MAX_APDU_LENGTH_ACCEPTED,          ASN_TYPE_UNSIGNED,  OFFSET(struct _DeviceObject, max_apdu_length_accepted)},
	{PROTOCOL_OBJECT_TYPES_SUPPORTED, 	ASN_TYPE_BIT_STRING, 	OFFSET(struct _DeviceObject, protocol_object_types_supported), (128/8)},
	{PROTOCOL_SERVICES_SUPPORTED, 		ASN_TYPE_BIT_STRING, 	OFFSET(struct _DeviceObject, protocol_services_supported), (64/8)},

	{VENDOR_IDENTIFIER,	ASN_TYPE_UNSIGNED, 		OFFSET(struct _DeviceObject, vendor_identifier), 2},// RO
	{_VENDOR_NAME,		ASN_TYPE_STRING, 		OFFSET(struct _DeviceObject, vendor_name)},// RO
	{MODEL_NAME,	 	ASN_TYPE_STRING, 		OFFSET(struct _DeviceObject, model_name)},// RO
	{FIRMWARE_REVISION,	ASN_TYPE_STRING, 		OFFSET(struct _DeviceObject, firmware_revision)},// RO
	{SERIAL_NUMBER,		ASN_TYPE_STRING, 		OFFSET(struct _DeviceObject, serial_number)},// RO

	{LOCAL_DATE, ASN_TYPE_DATE, 		OFFSET(struct _DeviceObject, local_date)},
	{LOCAL_TIME, ASN_TYPE_TIME, 		OFFSET(struct _DeviceObject, local_time)},
	{LOCATION,	 ASN_TYPE_STRING, 		OFFSET(struct _DeviceObject, location)},// RW
	{DESCRIPTION,ASN_TYPE_STRING, 		OFFSET(struct _DeviceObject, description)},// RW

	{SYSTEM_STATUS,			ASN_TYPE_ENUMERATED,OFFSET(struct _DeviceObject, system_status),1},
	{LAST_RESTART_REASON, 	ASN_TYPE_ENUMERATED,OFFSET(struct _DeviceObject, last_restart_reason), 1},
	{DATABASE_REVISION, 	ASN_TYPE_UNSIGNED,	OFFSET(struct _DeviceObject, database_revision), sizeof(uint32_t)},
    //{OBJECT_LIST, ASN_, OFFSET(Device_t, object_list), },// callback
};
static const PropertySpec_t AnalogInputObject [] = {
    {PRESENT_VALUE, ASN_TYPE_REAL,      OFFSET(struct _AnalogInputObject, present_value)},
    {UNITS, ASN_TYPE_ENUMERATED,        OFFSET(struct _AnalogInputObject, units), 2},
};
static const PropertySpec_t AnalogOutputObject[] = {
    {PRESENT_VALUE, ASN_TYPE_REAL,      OFFSET(struct _AnalogOutputObject, present_value), sizeof(REAL), EX|RW},// RW
    {UNITS, ASN_TYPE_ENUMERATED,        OFFSET(struct _AnalogOutputObject, units),2},
    {RELINQUISH_DEFAULT, ASN_TYPE_REAL, OFFSET(struct _AnalogOutputObject, relinquish_default)},
};
static const PropertySpec_t AnalogValueObject [] = {
    {PRESENT_VALUE, ASN_TYPE_REAL,      OFFSET(struct _AnalogValueObject, present_value),sizeof(REAL), EX|RW},
    {UNITS, ASN_TYPE_ENUMERATED,        OFFSET(struct _AnalogValueObject, units),2},
    {RELINQUISH_DEFAULT, ASN_TYPE_REAL, OFFSET(struct _AnalogValueObject, relinquish_default)},
};
static const PropertySpec_t BinaryInputObject [] = {
    {PRESENT_VALUE, ASN_TYPE_ENUMERATED,OFFSET(struct _BinaryInputObject, present_value), 1},
    {POLARITY, ASN_TYPE_ENUMERATED,     OFFSET(struct _BinaryInputObject, polarity), 1},
};
static const PropertySpec_t BinaryOutputObject[] = {
    {PRESENT_VALUE, ASN_TYPE_ENUMERATED,OFFSET(struct _BinaryOutputObject, present_value), 1, EX|RW}, // RW
    {POLARITY, ASN_TYPE_ENUMERATED,     OFFSET(struct _BinaryOutputObject, polarity), 1},
    {RELINQUISH_DEFAULT, ASN_TYPE_ENUMERATED,OFFSET(struct _BinaryOutputObject, relinquish_default), 1},
};
static const PropertySpec_t BinaryValueObject [] = {
    {PRESENT_VALUE, ASN_TYPE_ENUMERATED,OFFSET(struct _BinaryValueObject, present_value),1},
    {POLARITY, ASN_TYPE_ENUMERATED,     OFFSET(struct _BinaryValueObject, polarity),1},
};
static const PropertySpec_t CommandObject     [] = {
    {PRESENT_VALUE, ASN_TYPE_UNSIGNED,OFFSET(struct _CommandObject, present_value), sizeof(uint32_t), EX|RW},
    {IN_PROCESS, ASN_TYPE_BOOLEAN,    OFFSET(struct _CommandObject, in_process), sizeof(bool)},
    {ALL_WRITES_SUCCESSFUL, ASN_TYPE_BOOLEAN,    OFFSET(struct _CommandObject, all_writes_successful), sizeof(bool)},
//    {ACTION, 0, OFFSET(struct _BACnetCommandObject, action), RO|ARRAY},
};
static const PropertySpec_t CalendarObject [] = {
    {PRESENT_VALUE, ASN_TYPE_BOOLEAN, OFFSET(struct _CalendarObject, present_value), sizeof(bool)},
};

#define T(name) {name, sizeof(name)/sizeof(PropertySpec_t), sizeof(struct _##name)}
#define OBJECT_CLASS_COUNT 64
const DeviceObjectClass device_object_classes[OBJECT_CLASS_COUNT] = {
[ANALOG_INPUT]  = T(AnalogInputObject),
[ANALOG_OUTPUT] = T(AnalogOutputObject),
[ANALOG_VALUE]  = T(AnalogValueObject),
[BINARY_INPUT]  = T(BinaryInputObject),
[BINARY_OUTPUT] = T(BinaryOutputObject),
[BINARY_VALUE]  = T(BinaryValueObject),
[COMMAND]       = T(CommandObject),
[_DEVICE]       = T(DeviceObject),
//[SCHEDULE]      = T(CommandObject),
};
const DeviceObjectClass* object_classes_lookup(uint8_t type_id)
{
    if (type_id<OBJECT_CLASS_COUNT && device_object_classes[type_id].pspec!=0) {
        return &device_object_classes[type_id];
    }
    return NULL;
}

#undef T
