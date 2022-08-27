// BACnetEngineeringUnits
#ifndef BACnet_OBJECT_H
#define BACnet_OBJECT_H
#include "cmsis_os.h"
#include "bacnet.h"
#include "bacnet_units.h"
/*! \todo g_binding -- устанавливает связи между объектами */

/* Простые типы данных */
/*
G_PARAM_READABLE -- the parameter is readable
G_PARAM_WRITABLE -- the parameter is writable
G_PARAM_READWRITE --alias for G_PARAM_READABLE | G_PARAM_WRITABLE
G_PARAM_CONSTRUCT --the parameter will be set upon object construction
*/


#define RO (1<<10)
#define WO (2<<10)
#define RW (3<<10)
#define ARRAY    (1<<9)
#define OPTIONAL (1<<8)
#define SIZE(n)  (n)

#define ASN_TYPE_CHOICE 0xD0
#define ASN_TYPE_ANY    0xE0
//typedef struct _ObjectDesc ObjectDesc;
typedef struct _PropertySpec PropertySpec_t;
typedef struct _ParamSpec ParamSpec_t;
struct _ParamSpec {
    uint16_t asn_type;
    uint16_t offset;
    uint16_t flags;
    uint16_t size;
    const struct _ParamSpec *ref;
};
struct _PropertySpec {
    uint32_t prop_id:22;//enum _BACnetPropertyIdentifier prop_id;
    uint32_t asn_type:10;
    uint16_t offset;
    uint16_t flags;
    uint16_t size;
};
#define  LIST_OF(type) BACnetLIST*
#define ARRAY_OF(type) struct { uint32_t count; type* array; }
#define OFFSET(type, member) ((unsigned long)&((type*)0)->member)
#define DEF(name) name##_desc, sizeof(name##_desc)/sizeof(ParamSpec_t)


PropertySpec_t* bacnet_object_find_property (BACnetObject* object,  uint32_t property_id);
BACnetObject*   bacnet_object_new_with_properties (uint16_t type_id,
                              uint32_t n_properties,
                              const uint32_t property_id[],
                              const BACnetValue values[]);

BACnetObject*   bacnet_object_new   (uint16_t type_id);
void bacnet_object_ref   (BACnetObject* object);
void bacnet_object_unref (BACnetObject* object);
/* Emits a "notify" signal for the property specified by pspec on object */
void bacnet_object_notify(BACnetObject* object, uint32_t property_id);
/* свойство должно быть одного типа или определено преобразование между типами данных */
void bacnet_object_bind_property(BACnetObject* src_object, uint32_t property_id1, BACnetObject* dst_object, uint32_t property_id2);
int  bacnet_property_validate (const BACnetValue *value, const ParamSpec_t *pspec);
int  bacnet_property_get (BACnetObject* object,
            uint32_t property_id, uint16_t property_array_index, BACnetValue * value);
int  bacnet_property_set (BACnetObject* object,
            uint32_t property_id, uint16_t property_array_index, const BACnetValue * value);
BACnetLIST* bacnet_value_list_parse(BACnetLIST* list, void* data, const ParamSpec_t* desc, int size);
void bacnet_event_rise(BACnetObject* instance, enum _BACnetEventState event_state);

size_t   bacnet_paramspec_encode(uint8_t* buf, void* obj, const ParamSpec_t* pspec, int size);



/*!
    В контроллере тип REAL может быть представлен числами с фиксированной точкой
 */

typedef enum _BACnetPolarity {
    NORMAL,  REVERSE
} BACnetPolarity;
typedef enum _BACnetBinaryPV { INACTIVE, ACTIVE } BACnetBinaryPV;
enum _BACnetDeviceStatus    {
    OPERATIONAL,
    OPERATIONAL_READ_ONLY,
    DOWNLOAD_REQUIRED,
    DOWNLOAD_IN_PROGRESS,
    NON_OPERATIONAL,
    BACKUP_IN_PROGRESS
};
enum _BACnetDoorStatus      {
    CLOSED, OPENED, UNKNOWN, DOOR_FAULT, UNUSED
};
enum _BACnetLockStatus      {
    LOCKED, UNLOCKED
};
struct _BACnetDailySchedule {
    BACnetLIST* list;// (_BACnetTimeValues)
};

typedef struct _DeviceObjectClass DeviceObjectClass;
struct _DeviceObjectClass {
    const PropertySpec_t* desc;
    uint16_t property_list_length;
    uint16_t size;
};

// \todo убрать слово _BACnet
// описание этих объектов может подгружаться из файла XML

struct _BACnetAnalogInputObject {// 12.2 Analog Input Object Type
    BACnetObject object;
    REAL present_value;
    BACnetEngineeringUnits units;
};
struct _BACnetAnalogOutputObject{// 12.3 Analog Output Object Type
    BACnetObject object;
    REAL present_value;
    BACnetEngineeringUnits units;
    REAL relinquish_default;
};
struct _BACnetAnalogValueObject {// 12.4 Analog Value Object Type
    BACnetObject object;
    REAL present_value;
    BACnetEngineeringUnits units;
};
struct _BACnetAveragingObject   {// 12.5 Averaging Object Type
    BACnetObject object;
    REAL average_value;
    REAL minimum_value;
    REAL maximum_value;
    //BACnetDateTime
};
struct _BACnetBinaryOutputObject{// 12.6 Binary Input Object Type
    BACnetObject object;
    BACnetBinaryPV present_value;
    BACnetPolarity polarity;
    BACnetBinaryPV relinquish_default;
};
struct _BACnetBinaryInputObject {// 12.7 Binary Output Object Type
    BACnetObject instance;
    BACnetBinaryPV present_value;
    BACnetPolarity polarity;
};
struct _BACnetBinaryValueObject {// 12.8 Binary Value Object Type
    BACnetObject instance;
    BACnetBinaryPV present_value;
    BACnetPolarity polarity;
};
struct _BACnetCalendarObject    {// 12.9 Calendar Object Type
    BACnetObject instance;
    BOOLEAN present_value; ///< TRUE if the current date is in the Date_List and FALSE if it is not.
    BACnetLIST* date_list; ///< of BACnetCalendarEntry
};
struct _BACnetActionCommand {
    // см BACnetDeviceObjectPropertyValue_t
    BACnetObjectIdentifier device_identifier;// [0] BACnetObjectIdentifier OPTIONAL,
    BACnetObjectIdentifier object_identifier;// [1] BACnetObjectIdentifier,
    enum _BACnetPropertyIdentifier property_identifier;// [2] BACnetPropertyIdentifier,
    uint16_t property_array_index;           // [3] Unsigned OPTIONAL, -- used only with array datatype
    BACnetValue property_value;              // [4] ABSTRACT-SYNTAX.&Type,
    uint8_t priority;                        // [5] Unsigned (1..16) OPTIONAL, -- used only when property is commandable

    uint32_t post_delay;                     // [6] Unsigned OPTIONAL,
    BOOLEAN quit_on_failure;                 // [7] BOOLEAN,
    BOOLEAN write_successful;                // [8] BOOLEAN
};
struct _BACnetActionList    {
    LIST_OF(struct _BACnetActionCommand) action;
};
struct _BACnetCommandObject     {// 12.10 Command Object Type
    BACnetObject instance;
    Unsigned present_value;//! (W)
    BOOLEAN in_process;
    BOOLEAN all_writes_successful;
    ARRAY_OF(struct _BACnetActionList) action;
};


typedef struct _BACnetService BACnetService_t;
typedef struct _BACnetDeviceObject Device_t;
typedef struct _BACnetAddr BACnetAddr_t;
typedef struct _tree tree_t;
struct _BACnetService {
    int (*indication)(Device_t* device, BACnetAddr_t* source_address, BACnetValue* node);//!< представление данных
    int (*confirm)   (Device_t* device, BACnetValue* node);//!< получение отклика
};
typedef struct _BACnetAddress BACnetAddress_t;
struct _BACnetAddress {
    uint16_t network_number;//!< A value of 0 indicates the local network
	OctetString MAC_address;
};
typedef struct _DataLink DataLink_t;
struct _BACnetDeviceObject      {// 12.11 Device Object Type
    BACnetObject instance;
    Unsigned16 vendor_identifier;
    CharacterString vendor_name;
    CharacterString model_name;
    CharacterString firmware_revision;
    CharacterString application_software_version;
//    CharacterString serial_number;
    CharacterString location;
    struct {
        Unsigned version;
        Unsigned revision;
        Unsigned max_apdu_length_accepted;
        Unsigned max_segments_accepted;
        BACnetServicesSupported services_supported;
        BACnetObjectTypesSupported object_type_supported;
        enum _BACnetSegmentation segmentation_supported;

    } protocol;
//BACnetAddressBinding
//
    enum _BACnetDeviceStatus system_status;
    Unsigned database_revision;
    tree_t */* LIST_OF(BACnetAddressBinding_t)*/ Device_Address_Binding;
     LIST_OF(BACnetRecipient_t)      Time_Synchronization_Recipients;/*!< Device may automatically send a TimeSynchronization request
                but only to the devices or addresses listed. */
    ARRAY_OF(BACnetObjectIdentifier) Configuration_Files;
	ARRAY_OF(BACnetObjectIdentifier) Object_List;// список объектов

	BACnetService_t *service;// список сервисов
	BACnetService_t *unconfirmed_service;// список сервисов
	tree_t *tree;// дерево объектов
	struct _DataUnit apdu_transfer;
	struct _DataUnit apdu_requesting_transfer;
	uint8_t* apdu_requesting_buffer;

	osAsyncQueue_t apdu_requesting_queue;// очередь запросов от прикладного уровня к APDU? в очередь должны попадать объекты выделенные g_slice_alloc
	osService_t* apdu_requesting_service;
	osService_t* apdu_responding_service;
//	osService_t* dev_service;

	uint8_t num_ports;//!< число портов доступных для рутения
	DataLink_t** network_ports;
	tree_t *arp_table;
//	DataUnit_t transfer;
//	void* dev_service;
};



struct _EventEnrollmentObject   {// 12.12 Event Enrollment Object Type
    BACnetObject        object;
    enum _BACnetEventType   event_type;
    enum _BACnetNotifyType  Notify_Type;
    Unsigned                Notification_Class;
    BACnetCHOICE/* BACnetEventParameter */    Event_Parameters;
    BACnetDeviceObjectPropertyReference_t Object_Property_Reference;
    BACnetEventState        Event_State;
    BACnetEventTransitionBits Event_Enable;
    BACnetEventTransitionBits Acked_Transitions;
    BACnetTimeStamp_t event_time_stamps[3];
    BOOLEAN event_detection_enable;
    enum _BACnetReliability   Reliability;
};
struct _BACnetFileObject        {// 12.13 File Object Type
    BACnetObject object;
    CharacterString file_type;
    Unsigned file_size;
    BACnetDateTime_t modification_date;
    BOOLEAN archive;
    BOOLEAN read_only;
//    BACnetFileAccessMethod file_access_method;
};
struct _BACnetGroupObject       {// 12.14 Group Object Type
    BACnetObject instance;
    BACnetLIST* present_value;// (ReadAccessResult)
    BACnetLIST* list_of_group_members;// (ReadAccessSpecification)
};

struct _MultistateInputObject   {// 12.18 Multi-state Input Object Type
    BACnetObject instance;
    Unsigned present_value;
    Unsigned number_of_states;
};
struct _MultistateOutputObject  {// 12.19 Multi-state Output Object Type
    BACnetObject instance;
    Unsigned present_value;
    Unsigned number_of_states;
    Unsigned relinquish_default;
};
struct _MultistateValueObject   {// 12.20 Multi-state Value Object Type
    BACnetObject instance;
    Unsigned present_value;
    Unsigned number_of_states;
};
struct _NotificationClassObject {// 12.21 Notification Class Object Type
    BACnetObject instance;
    Unsigned    Notification_Class;
    BACnetLIST*/* of BACnetDestination */ Recipient_List;
};
struct _BACnetProgramObject     {// 12.22 Program Object Type
    BACnetObject instance;
    enum _BACnetProgramState program_state;
    enum _BACnetProgramRequest     program_change;
    CharacterString          program_location;
};

struct _BACnetScheduleObject    {// 12.24 Schedule Object Type
    BACnetObject instance;
    BACnetDateRange_t   effective_period;
    BACnetDailySchedule weekly_schedule[7];
    ARRAY_OF(BACnetSpecialEvent)       exception_schedule;
    BACnetValue present_value;
    BACnetValue schedule_default;
    BACnetLIST* list_of_object_property_references;// (BACnetDeviceObjectPropertyReference)
};
struct _LargeAnalogValueObject  {// 12.39 Large Analog Value Object Type
    BACnetObject instance;
    DOUBLE present_value;
    enum _BACnetEngineeringUnits units;
};
struct _BitStringValueObject    {// 12.40 BitString Value Object Type
    BACnetObject instance;
    BitString present_value;
    ARRAY_OF(CharacterString) bit_text;
    BitString bit_mask;
};
struct _OctetStringValueObject  {// 12.41 OctetString Value Object Type
    BACnetObject instance;
    OctetString present_value;
};
struct _BACnetTimeValueObject   {// 12.42 Time Value Object Type
    BACnetObject instance;
    Time present_value;
};
struct _IntegerValueObject      {// 12.43 Integer Value Object Type
    BACnetObject instance;
};
struct _PositiveIntegerValueObject {// 12.44 Positive Integer Value Object Type
    BACnetObject instance;
};

#endif // BACnet_OBJECT_H
