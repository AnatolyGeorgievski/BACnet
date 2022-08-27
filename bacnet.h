/*! bacnet_object - совокупность свойств, часть из которых об§зательны
Object_Identifier;
Object_Name
Object_Type
Property_List
 bacnet_service

таблица сервисов (до 40 шт):
статическа€ регистраци€ сервисов в системе
дл€ каждого сервиса определ€етс€:
структура запроса, с подтверждением или без
структура положительного ответа,
структура отрицательного ответа

->
Status_Flags
Event_State
Out_Of_Service BOOLEAN
Present_Value
Units
intrinsic reporting (optional) {
    event_algorithm
}
reliability-evaluation {
    fault_algorithm
}


ANSI/ASHRAE Standard 135-2016
Copyrighted material licensed to Anatoly Georgievskii on 2017-06-14 for licensee's use only.
All rights reserved. No further reproduction or distribution is permitted.
Distributed by Techstreet for ASHRAE, www.techstreet.com21. FORMAL DESCRIPTION OF APPLICATION PROTOCOL DATA UNITS


 */
#ifndef BACnet_H
#define BACnet_H
#include <stdint.h>
//#include <glib.h>
#include <stdbool.h>
//#include <r3_slist.h>
/*! типы данных дл€ описани€ объектов BACnet */
//typedef char*    CharacterString;
typedef uint32_t Unsigned;
typedef uint16_t Unsigned16;
typedef uint32_t Unsigned32;
typedef uint32_t Time;
typedef uint32_t Date;
typedef int32_t  INTEGER;
//typedef bool     BOOLEAN;
typedef float    REAL;
typedef double   DOUBLE;

typedef struct _CharacterString CharacterString;
typedef struct _OctetString OctetString;
typedef struct _BitString BitString;

typedef uint8_t  BACnetEventTransitionBits;
// по сути это BitString флаги
typedef uint64_t BACnetServicesSupported;
typedef uint64_t BACnetObjectTypesSupported;


typedef struct _BACnetARRAY     BACnetARRAY;
typedef struct _BACnetCHOICE    BACnetCHOICE;
typedef struct _BACnetOID       BACnetObjectIdentifier;

typedef struct _BACnetStatusFlags BACnetStatusFlags;
typedef struct _BACnetObject    BACnetObject;

typedef struct _BACnetTimeStamp BACnetTimeStamp_t;
typedef struct _BACnetDateTime  BACnetDateTime_t;
typedef struct _BACnetDateRange BACnetDateRange_t;
typedef struct _BACnetWeekNDay  BACnetWeekNDay_t;
typedef struct _BACnetTimeValues    BACnetTimeValues_t;
typedef struct _BACnetDailySchedule BACnetDailySchedule;
typedef struct _BACnetSpecialEvent  BACnetSpecialEvent;

typedef struct _BACnetPropertyValue     BACnetPropertyValue_t;
typedef struct _BACnetPropertyReference BACnetPropertyReference_t;
typedef struct _BACnetObjectPropertyValue       BACnetObjectPropertyValue_t;
typedef struct _BACnetObjectPropertyReference   BACnetObjectPropertyReference_t;
typedef struct _BACnetDeviceObjectReference     BACnetDeviceObjectReference_t;
typedef struct _BACnetDeviceObjectPropertyReference BACnetDeviceObjectPropertyReference_t;

//typedef struct _BACnetValue     BACnetValue;

typedef enum _BACnetAction      BACnetAction;
//typedef enum _BACnetAccessEvent BACnetAccessEvent;
//typedef enum _BACnetAuthenticationStatus BACnetAuthenticationStatus;
//typedef enum _BACnetAuthorizationMode BACnetAuthorizationMode;
//typedef enum _BACnetDoorValue   BACnetDoorValue;
typedef enum _BACnetEventState  BACnetEventState;
typedef enum _BACnetEngineeringUnits BACnetEngineeringUnits;
typedef enum _BACnetFileAccessMethod BACnetFileAccessMethod;
typedef enum _BACnetLifeSafetyState BACnetLifeSafetyState;
typedef enum _BACnetLifeSafetyMode  BACnetLifeSafetyMode;
typedef enum _BACnetNodeType    BACnetNodeType;
typedef enum _BACnetNotifyType  BACnetNotifyType;
typedef enum _BACnetObjectType  BACnetObjectType;
typedef enum _BACnetPropertyIdentifier BACnetPropertyIdentifier_t;
typedef enum _BACnetTimerState  BACnetTimerState_t;
typedef enum _BACnetReliability BACnetReliability_t;



struct _BACnetStatusFlags {
    int out_of_service:1; //!< флаг, означает что можно записать
    int overridden:1;
    int fault:1;
    int in_alarm:1;
};
typedef struct _BACnetTime  BACnetTime;
typedef struct _BACnetDate  BACnetDate;
typedef struct _BACnetList  BACnetLIST;
//typedef struct _BACnetNode  BACnetNode;
typedef struct _BACnetValue BACnetValue;
//typedef struct _BACnetOID   BACnetObjectIdentifier;
//typedef union _Value  BacValue;
struct _BACnetOID   {
    uint32_t instance: 22;
    uint32_t type    : 10;
};

struct _BACnetDate  {//  [APPLICATION 10] OCTET STRING (SIZE(4)) -- see Clause 20.2.12
/*
-- first octet year minus 1900, X'FF' = unspecified
-- second octet month (1.. 14), 1 = January
--                              13 = odd months
--                              14 = even months
--                              X'FF' = unspecified
-- third octet day of month (1..34), 32 = last day of month
--                                   33 = odd days of month
--                                   34 = even days of month
--                                   X'FF' = unspecified
-- fourth octet day of week (1..7)   1 = Monday
--                                   7 = Sunday
--                                   X'FF' = unspecified
*/
    uint8_t day_of_week;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};
struct _BACnetTime  {//  [APPLICATION 11] OCTET STRING (SIZE(4)) -- see Clause 20.2.13
    uint8_t hundredths; //!< 0..99, X'FF' = unspecified
    uint8_t second;     //!< 0..59, X'FF' = unspecified
    uint8_t minute;     //!< 0..59, X'FF' = unspecified
    uint8_t hour;       //!< 0..23, X'FF' = unspecified
};
union  _Value {
    uint32_t u;
    int32_t b;
    int32_t i;
    float f;
    char* s;
    void* ptr;
    uint8_t* octets;
    BACnetObjectIdentifier oid;
    BACnetLIST* list;
    BACnetTime  time;
    struct _BACnetValue* node;
    union _Value *array; // массив значений фиксированной длины
};
struct _BACnetList  {
    union _Value value;
    BACnetLIST *next;
};
struct _BACnetCHOICE  {
    union _Value value;
    uint32_t choice_id;
};

struct _BACnetValue {
    uint8_t tag;//!< содержит признак длины и типа данных
    uint8_t context_id;//!< содержит тип данных или контекст
    uint16_t length;
    union _Value value;
};

struct _BACnetARRAY {
    unsigned int count;
    void* base;
};
struct _OctetString {
    uint32_t length;
    uint8_t* data;
};
struct _CharacterString {
    uint32_t length;
    uint8_t* data;
};
struct _BitString   {
    unsigned int bits;
    uint8_t* data;
};

struct _BACnetDateTime {
    Date date;
    Time time;
};
struct _BACnetDateRange {
    Date start_date;
    Date end_date;
};
struct _BACnetWeekNDay {
    uint8_t month;
    uint8_t week_of_month;
    uint8_t day_of_week;
};
struct _BACnetTimeStamp {
    int choice;
    union {
        Time time;
        Unsigned sequence_number;
        BACnetDateTime_t datetime;
    };
};
struct _BACnetTimeValues {
    Time time;
    BACnetValue value;
};
/*
0 = Null
1 = Boolean
2 = Unsigned Integer
3 = Signed Integer (2's complement notation)
4 = Real (ANSI/IEEE-754 floating point)
5 = Double (ANSI/IEEE-754 double precision floating point)
6 = Octet String
7 = Character String
8 = Bit String
9 = Enumerated
10 = Date
11 = Time
12 = BACnetObjectIdentifier
13, 14, 15 = Reserved for ASHRAE
Note that all currently defined BACnet Application datatypes are primitively encoded
*/

/*
int bacnet_AddListElement(BACnetObjectIdentifier oid, BACnetPropertyIdentifier prop_id, uint16_t index, BACnetLIST list, void* result);
int bacnet_RemoveListElement(BACnetObjectIdentifier oid, BACnetPropertyIdentifier prop_id, uint16_t index, BACnetLIST list, void* result);
int bacnet_CreateObject(BACnetObjectIdentifier oid, BACnetPropertyIdentifier prop_id, uint16_t index, BACnetLIST list, void* result);
int bacnet_DeleteObject(BACnetObjectIdentifier oid, void* result);
int bacnet_ReadProperty(BACnetObjectIdentifier oid, BACnetPropertyIdentifier prop_id, uint16_t index, void* result);
int bacnet_ReadPropertyMultiple(BACnetLIST list, void* result);
int bacnet_WriteProperty(BACnetLIST list, void* result);

*/
#if 0
enum {
    BACNET_TAG_NULL=0,
    BACNET_TAG_BOOL=1,
    BACNET_TAG_UINT=2,
    BACNET_TAG_INT=3,
    BACNET_TAG_REAL=4,
    BACNET_TAG_DOUBLE=5,
    BACNET_TAG_OCTETS=6,
    BACNET_TAG_STRING=7,
    BACNET_TAG_BIT_STRING=8,
    BACNET_TAG_ENUMERATED=9,
    BACNET_TAG_DATE=10,
    BACNET_TAG_TIME=11,
    BACNET_TAG_OID=12,
    // 13-15 reserved
};
#endif // 0
enum _BACnetAccessEvent {
AccessEvent_none=0,
granted=1,
muster=2,
passback_detected=3,
AccessEvent_duress=4,
trace=5,
lockout_max_attempts=6,
lockout_other=7,
lockout_relinquished=8,
locked_by_higher_priority=9,
out_of_service=10,
out_of_service_relinquished=11,
accompaniment_by=12,
authentication_factor_read=13,
AccessEvent_authorization_delayed=14,
AccessEvent_verification_required=15,
no_entry_after_granted=16,
// Enumerated values 128-511 are used for events which indicate that access has been denied.
denied_deny_all=128,
denied_unknown_credential=129,
denied_authentication_unavailable=130,
denied_authentication_factor_timeout=131,
denied_incorrect_authentication_factor=132,
denied_zone_no_access_rights=133,
denied_point_no_access_rights=134,
denied_no_access_rights=135,
denied_out_of_time_range=136,
denied_threat_level=137,
denied_passback=138,
denied_unexpected_location_usage=139,
denied_max_attempts=140,
denied_lower_occupancy_limit=141,
denied_upper_occupancy_limit=142,
denied_authentication_factor_lost=143,
denied_authentication_factor_stolen=144,
denied_authentication_factor_damaged=145,
denied_authentication_factor_destroyed=146,
denied_authentication_factor_disabled=147,
denied_authentication_factor_error=148,
denied_credential_unassigned=149,
denied_credential_not_provisioned=150,
denied_credential_not_yet_active=151,
denied_credential_expired=152,
denied_credential_manual_disable=153,
denied_credential_lockout=154,
denied_credential_max_days=155,
denied_credential_max_uses=156,
denied_credential_inactivity=157,
denied_credential_disabled=158,
denied_no_accompaniment=159,
denied_incorrect_accompaniment=160,
denied_lockout=161,
denied_verification_failed=162,
denied_verification_timeout=163,
denied_other=164,
//...
};
enum _BACnetAction {
direct =(0),
reverse =(1)
};
enum _BACnetAuthenticationStatus {
Auth_not_ready =(0),
ready =(1),
Auth_disabled =(2),
waiting_for_authentication_factor =(3),
waiting_for_accompaniment =(4),
waiting_for_verification =(5),
in_progress =(6)
};
enum _BACnetAuthorizationMode {
authorize =(0),
grant_active =(1),
deny_all =(2),
verification_required =(3),
authorization_delayed =(4),
none =(5),
// ...
};
enum _BACnetDoorValue {
lock =(0),
unlock =(1),
pulse_unlock =(2),
extended_pulse_unlock =(3)
};
enum _BACnetEventState{
EventState_NORMAL=0,
EventState_FAULT=1,
EventState_OFFNORMAL=2,
EventState_HIGH_LIMIT=3,
EventState_LOW_LIMIT=4,
EventState_LIFE_SAFETY_ALARM=5,
};
enum _BACnetEventType {
change_of_bitstring = (0),
change_of_state = (1),
change_of_value = (2),
command_failure = (3),
floating_limit = (4),
out_of_range = (5),
// complex_event_type (6), see comment below
// context tag 7 is deprecated
change_of_life_safety = (8),
extended = (9),
buffer_ready = (10),
unsigned_range = (11),
// enumeration value 12 is reserved for future addenda
access_event = (13),
double_out_of_range = (14),
signed_out_of_range = (15),
unsigned_out_of_range = (16),
change_of_characterstring = (17),
change_of_status_flags = (18),
change_of_reliability = (19),
EventType_none = (20),
change_of_discrete_value = (21),
change_of_timer = (22),
// ...
};
enum _BACnetEventTransitionBits /* BIT STRING */{
to_offnormal =(1<<0),
to_fault =(1<<1),
to_normal =(1<<2)
};
enum _BACnetFileAccessMethod {
record_access =(0),
stream_access =(1)
};
enum _BACnetLifeSafetyState {
quiet=0,
pre_alarm=1,
_alarm=2,
fault=3,
fault_pre_alarm=4,
fault_alarm=5,
not_ready=6,
active=7,
tamper=8,
test_alarm=9,
test_active=10,
test_fault=11,
test_fault_alarm=12,
holdup=13,
duress=14,
tamper_alarm=15,
abnormal=16,
emergency_power=17,
delayed=18,
blocked=19,
local_alarm=20,
general_alarm=21,
supervisory=22,
test_supervisory=23,
// ...
};
enum _BACnetLifeSafetyMode {
off =(0),
on =(1),
test =(2),
manned =(3),
unmanned =(4),
armed =(5),
disarmed =(6),
prearmed =(7),
slow =(8),
fast =(9),
disconnected =(10),
enabled =(11),
disabled =(12),
automatic_release_disabled =(13),
LifeSafetyMode_default =(14),
//...
};

enum _BACnetNotifyType {
NotifyType_alarm =(0),
NotifyType_event =(1),
NotifyType_ack_notification =(2)
};
#ifndef R3_OBJECT
enum _BACnetObjectType {
ANALOG_INPUT = 0,
ANALOG_OUTPUT = 1,
ANALOG_VALUE = 2,
BINARY_INPUT = 3,
BINARY_OUTPUT = 4,
BINARY_VALUE = 5,
CALENDAR = 6,
COMMAND = 7,
_DEVICE = 8,
EVENT_ENROLLMENT = 9,
_FILE = 10,
_GROUP = 11,
LOOP = 12,
MULTI_STATE_INPUT = 13,
MULTI_STATE_OUTPUT = 14,
_NOTIFICATION_CLASS = 15,
PROGRAM = 16,
SCHEDULE = 17,
AVERAGING = 18,
MULTI_STATE_VALUE = 19,
TREND_LOG = 20,
LIFE_SAFETY_POINT = 21,
LIFE_SAFETY_ZONE = 22,
ACCUMULATOR = 23,
PULSE_CONVERTER = 24,
EVENT_LOG = 25,
GLOBAL_GROUP = 26,
TREND_LOG_MULTIPLE = 27,
LOAD_CONTROL = 28,
STRUCTURED_VIEW = 29,
ACCESS_DOOR = 30,
TIMER = 31,
ACCESS_CREDENTIAL = 32,
ACCESS_POINT = 33,
ACCESS_RIGHTS = 34,
ACCESS_USER = 35,
ACCESS_ZONE = 36,
CREDENTIAL_DATA_INPUT = 37,
NETWORK_SECURITY = 38,
BITSTRING_VALUE = 39,
CHARACTERSTRING_VALUE = 40,
DATE_PATTERN_VALUE = 41,
DATE_VALUE = 42,
DATETIME_PATTERN_VALUE = 43,
DATETIME_VALUE = 44,
INTEGER_VALUE = 45,
LARGE_ANALOG_VALUE = 46,
OCTETSTRING_VALUE = 47,
POSITIVE_INTEGER_VALUE = 48,
TIME_PATTERN_VALUE = 49,
TIME_VALUE = 50,
NOTIFICATION_FORWARDER = 51,
ALERT_ENROLLMENT = 52,
CHANNEL = 53,
LIGHTING_OUTPUT = 54,
BINARY_LIGHTING_OUTPUT = 55,
NETWORK_PORT = 56,
ELEVATOR_GROUP = 57,
ESCALATOR = 58,
LIFT = 59,
};
#endif // _BACnetObjectType
enum _BACnetProgramRequest {
    READY,      ///< ready for change request (the normal state)
    LOAD,       ///< request that the application program be loaded, if not already loaded
    RUN,        ///< request that the process begin executing, if not already running
    PAUSE,      ///< ????
    HALT,       ///< request that the process halt execution
    UNLOAD,     ///< request that the process halt execution and unload
    RESTART,    ///< request that the process restart at its initialization point
};
enum _BACnetProgramState {
    IDLE,       ///< process is not executing
    LOADING,    ///< application program being loaded
    RUNNING,    ///< process is currently executing
    WAITING,    ///< process is waiting for some external event
    HALTED,     ///< process is halted because of some error condition
    UNLOADING,  ///< process has been requested to terminate
};
typedef enum _BACnetPropertyIdentifier BACnetPropertyIdentifier;
enum _BACnetPropertyIdentifier {
ACKED_TRANSITIONS = 0,
ACK_REQUIRED = 1,
ACTION = 2,
ACTION_TEXT = 3,
ACTIVE_TEXT = 4,
ACTIVE_VT_SESSIONS = 5,
ALARM_VALUE = 6,
ALARM_VALUES = 7,
ALL = 8,
ALL_WRITES_SUCCESSFUL = 9,
APDU_SEGMENT_TIMEOUT = 10,
APDU_TIMEOUT = 11,
APPLICATION_SOFTWARE_VERSION = 12,
ARCHIVE = 13,
BIAS = 14,
CHANGE_OF_STATE_COUNT = 15,
CHANGE_OF_STATE_TIME = 16,
NOTIFICATION_CLASS = 17,
// __ this property deleted (18),
CONTROLLED_VARIABLE_REFERENCE = 19,
CONTROLLED_VARIABLE_UNITS = 20,
CONTROLLED_VARIABLE_VALUE = 21,
COV_INCREMENT = 22,
DATE_LIST = 23,
DAYLIGHT_SAVINGS_STATUS = 24,
DEADBAND = 25,
DERIVATIVE_CONSTANT = 26,
DERIVATIVE_CONSTANT_UNITS = 27,
DESCRIPTION = 28,
DESCRIPTION_OF_HALT = 29,
DEVICE_ADDRESS_BINDING = 30,
_DEVICE_TYPE = 31,// DEVICE_TYPE используетс€ в виндах
EFFECTIVE_PERIOD = 32,
ELAPSED_ACTIVE_TIME = 33,
ERROR_LIMIT = 34,
EVENT_ENABLE = 35,
EVENT_STATE = 36,
EVENT_TYPE = 37,
EXCEPTION_SCHEDULE = 38,
FAULT_VALUES = 39,
FEEDBACK_VALUE = 40,
FILE_ACCESS_METHOD = 41,
FILE_SIZE = 42,
FILE_TYPE = 43,
FIRMWARE_REVISION = 44,
HIGH_LIMIT = 45,
INACTIVE_TEXT = 46,
IN_PROCESS = 47,
INSTANCE_OF = 48,
INTEGRAL_CONSTANT = 49,
INTEGRAL_CONSTANT_UNITS = 50,
// __ formerly: issue_confirmed_notifications (51), removed in version 1 revision 4.
LIMIT_ENABLE = 52,
LIST_OF_GROUP_MEMBERS = 53,
LIST_OF_OBJECT_PROPERTY_REFERENCES = 54,
// __ enumeration value 55 is unassigned
LOCAL_DATE = 56,
LOCAL_TIME = 57,
LOCATION = 58,
LOW_LIMIT = 59,
MANIPULATED_VARIABLE_REFERENCE = 60,
MAXIMUM_OUTPUT = 61,
MAX_APDU_LENGTH_ACCEPTED = 62,
MAX_INFO_FRAMES = 63,
MAX_MASTER = 64,
MAX_PRES_VALUE = 65,
MINIMUM_OFF_TIME = 66,
MINIMUM_ON_TIME = 67,
MINIMUM_OUTPUT = 68,
MIN_PRES_VALUE = 69,
MODEL_NAME = 70,
MODIFICATION_DATE = 71,
NOTIFY_TYPE = 72,
NUMBER_OF_APDU_RETRIES = 73,
NUMBER_OF_STATES = 74,
OBJECT_IDENTIFIER = 75,
OBJECT_LIST = 76,
OBJECT_NAME = 77,
OBJECT_PROPERTY_REFERENCE = 78,
OBJECT_TYPE = 79,
_OPTIONAL = 80,
OUT_OF_SERVICE = 81,
OUTPUT_UNITS = 82,
EVENT_PARAMETERS = 83,
POLARITY = 84,
PRESENT_VALUE = 85,
PRIORITY = 86,
PRIORITY_ARRAY = 87,
PRIORITY_FOR_WRITING = 88,
PROCESS_IDENTIFIER = 89,
PROGRAM_CHANGE = 90,
PROGRAM_LOCATION = 91,
PROGRAM_STATE = 92,
PROPORTIONAL_CONSTANT = 93,
PROPORTIONAL_CONSTANT_UNITS = 94,
// __ formerly: protocol_conformance_class (95), removed in version 1 revision 2.
PROTOCOL_OBJECT_TYPES_SUPPORTED = 96,
PROTOCOL_SERVICES_SUPPORTED = 97,
PROTOCOL_VERSION = 98,
READ_ONLY = 99,
REASON_FOR_HALT = 100,
// __ formerly: recipient (101), removed in version 1 revision 4.
RECIPIENT_LIST = 102,
RELIABILITY = 103,
RELINQUISH_DEFAULT = 104,
REQUIRED = 105,
RESOLUTION = 106,
SEGMENTATION_SUPPORTED = 107,
SETPOINT = 108,
SETPOINT_REFERENCE = 109,
STATE_TEXT = 110,
STATUS_FLAGS = 111,
SYSTEM_STATUS = 112,
TIME_DELAY = 113,
TIME_OF_ACTIVE_TIME_RESET = 114,
TIME_OF_STATE_COUNT_RESET = 115,
TIME_SYNCHRONIZATION_RECIPIENTS = 116,
UNITS = 117,
UPDATE_INTERVAL = 118,
UTC_OFFSET = 119,
VENDOR_IDENTIFIER = 120,
_VENDOR_NAME = 121,
VT_CLASSES_SUPPORTED = 122,
WEEKLY_SCHEDULE = 123,
ATTEMPTED_SAMPLES = 124,
AVERAGE_VALUE = 125,
BUFFER_SIZE = 126,
CLIENT_COV_INCREMENT = 127,
COV_RESUBSCRIPTION_INTERVAL = 128,
// __ formerly: current_notify_time (129), removed in version 1 revision 3.
EVENT_TIME_STAMPS = 130,
LOG_BUFFER = 131,
LOG_DEVICE_OBJECT_PROPERTY = 132,
_ENABLE = 133, // log_enable was renamed to enable in version 1 revision 5
LOG_INTERVAL = 134,
MAXIMUM_VALUE = 135,
MINIMUM_VALUE = 136,
NOTIFICATION_THRESHOLD = 137,
// __ formerly: previous_notify_time (138), removed in version 1 revision 3.
PROTOCOL_REVISION = 139,
RECORDS_SINCE_NOTIFICATION = 140,
RECORD_COUNT = 141,
START_TIME = 142,
STOP_TIME = 143,
STOP_WHEN_FULL = 144,
TOTAL_RECORD_COUNT = 145,
VALID_SAMPLES = 146,
WINDOW_INTERVAL = 147,
WINDOW_SAMPLES = 148,
MAXIMUM_VALUE_TIMESTAMP = 149,
MINIMUM_VALUE_TIMESTAMP = 150,
VARIANCE_VALUE = 151,
ACTIVE_COV_SUBSCRIPTIONS = 152,
BACKUP_FAILURE_TIMEOUT = 153,
CONFIGURATION_FILES = 154,
DATABASE_REVISION = 155,
DIRECT_READING = 156,
LAST_RESTORE_TIME = 156,
MAINTENANCE_REQUIRED = 158,
MEMBER_OF = 159,
MODE = 160,
OPERATION_EXPECTED = 161,
SETTING = 162,
SILENCED = 163,
TRACKING_VALUE = 164,
ZONE_MEMBERS = 165,
LIFE_SAFETY_ALARM_VALUES = 166,
MAX_SEGMENTS_ACCEPTED = 167,
PROFILE_NAME = 168,
AUTO_SLAVE_DISCOVERY = 169,
MANUAL_SLAVE_ADDRESS_BINDING = 170,
SLAVE_ADDRESS_BINDING = 171,
SLAVE_PROXY_ENABLE = 172,
LAST_NOTIFY_RECORD = 173,
SCHEDULE_DEFAULT = 174,
ACCEPTED_MODES = 175,
ADJUST_VALUE = 176,
COUNT = 177,
COUNT_BEFORE_CHANGE = 178,
COUNT_CHANGE_TIME = 179,
COV_PERIOD = 180,
INPUT_REFERENCE = 181,
LIMIT_MONITORING_INTERVAL = 182,
LOGGING_OBJECT = 183,
LOGGING_RECORD = 184,
PRESCALE = 185,
PULSE_RATE = 186,
SCALE = 187,
SCALE_FACTOR = 188,
UPDATE_TIME = 189,
VALUE_BEFORE_CHANGE = 190,
VALUE_SET = 191,
VALUE_CHANGE_TIME = 192,
ALIGN_INTERVALS = 193,
// __ enumeration value 194 is unassigned
INTERVAL_OFFSET = 195,
LAST_RESTART_REASON = 196,
LOGGING_TYPE = 197,
// __ enumeration values 198_201 are unassigned
RESTART_NOTIFICATION_RECIPIENTS = 202,
TIME_OF_DEVICE_RESTART = 203,
TIME_SYNCHRONIZATION_INTERVAL = 204,
TRIGGER = 205,
UTC_TIME_SYNCHRONIZATION_RECIPIENTS = 206,
NODE_SUBTYPE = 207,
NODE_TYPE = 208,
STRUCTURED_OBJECT_LIST = 209,
SUBORDINATE_ANNOTATIONS = 210,
SUBORDINATE_LIST = 211,
ACTUAL_SHED_LEVEL = 212,
DUTY_WINDOW = 213,
EXPECTED_SHED_LEVEL = 214,
FULL_DUTY_BASELINE = 215,
// __ enumeration values 216_217 are unassigned
REQUESTED_SHED_LEVEL = 218,
SHED_DURATION = 219,
SHED_LEVEL_DESCRIPTIONS = 220,
SHED_LEVELS = 221,
STATE_DESCRIPTION = 222,
// __ enumeration values 223_225 are unassigned
DOOR_ALARM_STATE = 226,
DOOR_EXTENDED_PULSE_TIME = 227,
DOOR_MEMBERS = 228,
DOOR_OPEN_TOO_LONG_TIME = 229,
DOOR_PULSE_TIME = 230,
DOOR_STATUS = 231,
DOOR_UNLOCK_DELAY_TIME = 232,
LOCK_STATUS = 233,
MASKED_ALARM_VALUES = 234,
SECURED_STATUS = 235,
// __ enumeration values 236_243 are unassigned
ABSENTEE_LIMIT = 244,
ACCESS_ALARM_EVENTS = 245,
ACCESS_DOORS = 246,
ACCESS_EVENT = 247,
ACCESS_EVENT_AUTHENTICATION_FACTOR = 248,
ACCESS_EVENT_CREDENTIAL = 249,
ACCESS_EVENT_TIME = 250,
ACCESS_TRANSACTION_EVENTS = 251,
ACCOMPANIMENT = 252,
ACCOMPANIMENT_TIME = 253,
ACTIVATION_TIME = 254,
ACTIVE_AUTHENTICATION_POLICY = 255,
ASSIGNED_ACCESS_RIGHTS = 256,
AUTHENTICATION_FACTORS = 257,
AUTHENTICATION_POLICY_LIST = 258,
AUTHENTICATION_POLICY_NAMES = 259,
AUTHENTICATION_STATUS = 260,
AUTHORIZATION_MODE = 261,
BELONGS_TO = 262,
CREDENTIAL_DISABLE = 263,
CREDENTIAL_STATUS = 264,
CREDENTIALS = 265,
CREDENTIALS_IN_ZONE = 266,
DAYS_REMAINING = 267,
ENTRY_POINTS = 268,
EXIT_POINTS = 269,
EXPIRY_TIME = 270,
EXTENDED_TIME_ENABLE = 271,
FAILED_ATTEMPT_EVENTS = 272,
FAILED_ATTEMPTS = 273,
FAILED_ATTEMPTS_TIME = 274,
LAST_ACCESS_EVENT = 275,
LAST_ACCESS_POINT = 276,
LAST_CREDENTIAL_ADDED = 277,
LAST_CREDENTIAL_ADDED_TIME = 278,
LAST_CREDENTIAL_REMOVED = 279,
LAST_CREDENTIAL_REMOVED_TIME = 280,
LAST_USE_TIME = 281,
LOCKOUT = 282,
LOCKOUT_RELINQUISH_TIME = 283,
// __ formerly: master_exemption (284), removed in version 1 revision 13
MAX_FAILED_ATTEMPTS = 285,
MEMBERS = 286,
MUSTER_POINT = 287,
NEGATIVE_ACCESS_RULES = 288,
NUMBER_OF_AUTHENTICATION_POLICIES = 289,
OCCUPANCY_COUNT = 290,
OCCUPANCY_COUNT_ADJUST = 291,
OCCUPANCY_COUNT_ENABLE = 292,
// __ formerly: occupancy_exemption (293), removed in version 1 revision 13
OCCUPANCY_LOWER_LIMIT = 294,
OCCUPANCY_LOWER_LIMIT_ENFORCED = 295,
OCCUPANCY_STATE = 296,
OCCUPANCY_UPPER_LIMIT = 297,
OCCUPANCY_UPPER_LIMIT_ENFORCED = 298,
// __ formerly: passback_exemption (299), removed in version 1 revision 13
PASSBACK_MODE = 300,
PASSBACK_TIMEOUT = 301,
POSITIVE_ACCESS_RULES = 302,
REASON_FOR_DISABLE = 303,
SUPPORTED_FORMATS = 304,
SUPPORTED_FORMAT_CLASSES = 305,
THREAT_AUTHORITY = 306,
THREAT_LEVEL = 307,
TRACE_FLAG = 308,
TRANSACTION_NOTIFICATION_CLASS = 309,
USER_EXTERNAL_IDENTIFIER = 310,
USER_INFORMATION_REFERENCE = 311,
// __ enumeration values 312_316 are unassigned
USER_NAME = 317,
USER_TYPE = 318,
USES_REMAINING = 319,
ZONE_FROM = 320,
ZONE_TO = 321,
ACCESS_EVENT_TAG = 322,
GLOBAL_IDENTIFIER = 323,
// __ enumeration values 324_325 are unassigned
VERIFICATION_TIME = 326,
BASE_DEVICE_SECURITY_POLICY = 327,
DISTRIBUTION_KEY_REVISION = 328,
DO_NOT_HIDE = 329,
KEY_SETS = 330,
LAST_KEY_SERVER = 331,
NETWORK_ACCESS_SECURITY_POLICIES = 332,
PACKET_REORDER_TIME = 333,
SECURITY_PDU_TIMEOUT = 334,
SECURITY_TIME_WINDOW = 335,
SUPPORTED_SECURITY_ALGORITHMS = 336,
UPDATE_KEY_SET_TIMEOUT = 337,
BACKUP_AND_RESTORE_STATE = 338,
BACKUP_PREPARATION_TIME = 339,
RESTORE_COMPLETION_TIME = 340,
RESTORE_PREPARATION_TIME = 341,
BIT_MASK = 342,
BIT_TEXT = 343,
IS_UTC = 344,
GROUP_MEMBERS = 345,
GROUP_MEMBER_NAMES = 346,
MEMBER_STATUS_FLAGS = 347,
REQUESTED_UPDATE_INTERVAL = 348,
COVU_PERIOD = 349,
COVU_RECIPIENTS = 350,
EVENT_MESSAGE_TEXTS = 351,
EVENT_MESSAGE_TEXTS_CONFIG = 352,
EVENT_DETECTION_ENABLE = 353,
EVENT_ALGORITHM_INHIBIT = 354,
EVENT_ALGORITHM_INHIBIT_REF = 355,
TIME_DELAY_NORMAL = 356,
RELIABILITY_EVALUATION_INHIBIT = 357,
FAULT_PARAMETERS = 358,
FAULT_TYPE = 359,
LOCAL_FORWARDING_ONLY = 360,
PROCESS_IDENTIFIER_FILTER = 361,
SUBSCRIBED_RECIPIENTS = 362,
PORT_FILTER = 363,
AUTHORIZATION_EXEMPTIONS = 364,
ALLOW_GROUP_DELAY_INHIBIT = 365,
CHANNEL_NUMBER = 366,
CONTROL_GROUPS = 367,
EXECUTION_DELAY = 368,
LAST_PRIORITY = 369,
WRITE_STATUS = 370,
PROPERTY_LIST = 371,
SERIAL_NUMBER = 372,
BLINK_WARN_ENABLE = 373,
DEFAULT_FADE_TIME = 374,
DEFAULT_RAMP_RATE = 375,
DEFAULT_STEP_INCREMENT = 376,
EGRESS_TIME = 377,
IN_PROGRESS = 378,
INSTANTANEOUS_POWER = 379,
LIGHTING_COMMAND = 380,
LIGHTING_COMMAND_DEFAULT_PRIORITY = 381,
MAX_ACTUAL_VALUE = 382,
MIN_ACTUAL_VALUE = 383,
POWER = 384,
TRANSITION = 385,
EGRESS_ACTIVE = 386,
APDU_LENGTH = 399,
IP_ADDRESS = 400,
IP_DEFAULT_GATEWAY = 401,
IP_DHCP_ENABLE = 402,
IP_DHCP_LEASE_TIME = 403,
IP_DHCP_LEASE_TIME_REMAINING = 404,
IP_DHCP_SERVER = 405,
IP_DNS_SERVER = 406,
IP_GLOBAL_ADDRESS = 407,
IP_MODE = 408,
IP_MULTICAST_ADDRESS = 409,
IP_NAT_TRAVERSAL = 410,
IP_SUBNET_MASK = 411,
IP_UDP_PORT = 412,
BBMD_ACCEPT_FD_REGISTRATIONS = 413,
BBMD_BROADCAST_DISTRIBUTION_TABLE = 414,
BBMD_FOREIGN_DEVICE_TABLE = 415,
CHANGES_PENDING = 416,
_COMMAND = 417,
FD_BBMD_ADDRESS = 418,
FD_SUBSCRIPTION_LIFETIME = 419,
LINK_SPEED = 420,
LINK_SPEEDS = 421,
LINK_SPEED_AUTONEGOTIATE = 422,
MAC_ADDRESS = 423,
NETWORK_INTERFACE_NAME = 424,
NETWORK_NUMBER = 425,
NETWORK_NUMBER_QUALITY = 426,
NETWORK_TYPE = 427,
ROUTING_TABLE = 428,
VIRTUAL_MAC_ADDRESS_TABLE = 429,
//
IPV6_MODE = 435,
IPV6_ADDRESS = 436,
IPV6_PREFIX_LENGTH = 437,
IPV6_UDP_PORT = 438,
IPV6_DEFAULT_GATEWAY = 439,
IPV6_MULTICAST_ADDRESS = 440,
IPV6_DNS_SERVER = 441,
IPV6_AUTO_ADDRESSING_ENABLE = 442,
IPV6_DHCP_LEASE_TIME = 443,
IPV6_DHCP_LEASE_TIME_REMAINING = 444,
IPV6_DHCP_SERVER = 445,
IPV6_ZONE_INDEX = 446,
// ...
PROTOCOL_LEVEL = 482,
REFERENCE_PORT = 483,
// ...
// -- The special property identifiers all, optional, and required are reserved for use in the
// -- ReadPropertyMultiple service or services not defined in this standard.
// --
// -- Enumerated values 0-511 are reserved for definition by ASHRAE. Enumerated values 512-4194303 may be used by
// -- others subject to the procedures and constraints described in Clause 23

/* \see http://www.bacnet.org/Addenda/Add-135-2016bk-ppr1-draft-4_chair_approved.pdf */
_RESERVED_PROPERTY_MAX =0xFFFFFFFF
};
enum _BACnetReliability {
NO_FAULT_DETECTED = 0,
NO_SENSOR = 1,
OVER_RANGE = 2,
UNDER_RANGE = 3,
OPEN_LOOP = 4,
SHORTED_LOOP = 5,
NO_OUTPUT = 6,
UNRELIABLE_OTHER = 7,
PROCESS_ERROR = 8,
MULTI_STATE_FAULT = 9,
CONFIGURATION_ERROR = 10,
// -- enumeration value 11 is reserved for a future addendum
COMMUNICATION_FAILURE = 12,
MEMBER_FAULT = 13,
MONITORED_OBJECT_FAULT = 14,
TRIPPED = 15,
ACTIVATION_FAILURE = 17,
RENEW_DHCP_FAILURE = 18,
RENEW_FD_REGISTRATION_FAILURE = 19,
RESTART_AUTO_NEGOTIATION_FAILURE = 20,
RESTART_FAILURE = 21,
PROPRIETARY_COMMAND_FAILURE = 22,
REFERENCED_OBJECT_FAULT = 24,
// ...
// -- Enumerated values 0-63 are reserved for definition by ASHRAE. Enumerated values
// -- 64-65535 may be used by others subject to the procedures and constraints described
// -- in Clause 23.
};
/*
enum _BACnetSegmentation {
segmented_both = 0,
segmented_transmit = 1,
segmented_receive = 2,
no_segmentation = 3
};*/
#if 0
enum _BACnetServicesSupported {
// Alarm and Event Services
ACKNOWLEDGE_ALARM = (0),
CONFIRMED_COV_NOTIFICATION = (1),
// CONFIRMED_COV_NOTIFICATION_MULTIPLE = (42),
CONFIRMED_EVENT_NOTIFICATION = (2),
GET_ALARM_SUMMARY = (3),
GET_ENROLLMENT_SUMMARY = (4),
// GET_EVENT_INFORMATION = (39),
// LIFE_SAFETY_OPERATION = (37),
SUBSCRIBE_COV = (5),
// SUBSCRIBE_COV_PROPERTY = (38),
// SUBSCRIBE_COV_PROPERTY_MULTIPLE = (41),
// File Access Services
ATOMIC_READ_FILE = (6),
ATOMIC_WRITE_FILE = (7),
// Object Access Services
ADD_LIST_ELEMENT = (8),
REMOVE_LIST_ELEMENT = (9),
CREATE_OBJECT = (10),
DELETE_OBJECT = (11),
READ_PROPERTY = (12),
READ_PROPERTY_MULTIPLE = (14),
// READ_RANGE = (35),
// WRITE_GROUP = (40),
WRITE_PROPERTY = (15),
WRITE_PROPERTY_MULTIPLE = (16),
// Remote Device Management Services
DEVICE_COMMUNICATION_CONTROL = (17),
CONFIRMED_PRIVATE_TRANSFER = (18),
CONFIRMED_TEXT_MESSAGE = (19),
REINITIALIZE_DEVICE = (20),
// Virtual Terminal Services
VT_OPEN = (21),
VT_CLOSE = (22),
VT_DATA = (23),
// Removed Services
// formerly: read_property_conditional = (13), removed in version 1 revision 12
// formerly: authenticate = (24), removed in version 1 revision 11
// formerly: request_key = (25), removed in version 1 revision 11
// Unconfirmed Services
I_AM = (26),
I_HAVE = (27),
UNCONFIRMED_COV_NOTIFICATION = (28),
// UNCONFIRMED_COV_NOTIFICATION_MULTIPLE = (43),
UNCONFIRMED_EVENT_NOTIFICATION = (29),
UNCONFIRMED_PRIVATE_TRANSFER = (30),
UNCONFIRMED_TEXT_MESSAGE = (31),
TIME_SYNCHRONIZATION = (32),
// UTC_TIME_SYNCHRONIZATION = (36),
WHO_HAS = (33),
WHO_IS = (34),
// Services added after 1995
READ_RANGE = (35), // Object Access Service
UTC_TIME_SYNCHRONIZATION = (36), // Remote Device Management Service
LIFE_SAFETY_OPERATION = (37), // Alarm and Event Service
SUBSCRIBE_COV_PROPERTY = (38), // Alarm and Event Service
GET_EVENT_INFORMATION = (39), // Alarm and Event Service
WRITE_GROUP = (40), // Object Access Services
// Services added after 2012
SUBSCRIBE_COV_PROPERTY_MULTIPLE = (41), // Alarm and Event Service
CONFIRMED_COV_NOTIFICATION_MULTIPLE = (42), // Alarm and Event Service
UNCONFIRMED_COV_NOTIFICATION_MULTIPLE = (43), // Alarm and Event Service
};
#endif // 0
enum _BACnetTimerState {
idle =(0),
running =(1),
expired =(2)
};
enum _BACnetVTClass {
DEFAULT_TERMINAL,
ANSI_X3_64,
DEC_VT52,
DEC_VT100,
DEC_VT220,
HP_700_94,
IBM_3130,
};
enum _BACnetRestartReason {
BACnetRestartReason_unknown =(0),
BACnetRestartReason_coldstart =(1),
BACnetRestartReason_warmstart =(2),
BACnetRestartReason_detected_power_lost =(3),
BACnetRestartReason_detected_powered_off =(4),
BACnetRestartReason_hardware_watchdog =(5),
BACnetRestartReason_software_watchdog =(6),
BACnetRestartReason_suspended =(7),
BACnetRestartReason_activate_changes =(8),
};

// Њроцедуры: BackUp-Restore
struct _BACnetObject {
    BACnetObjectIdentifier oid;
    volatile uint32_t ref_count; // счетчик ссылок на объект
//    BACnetObjectType type;
    CharacterString object_name;
// описание может
    CharacterString description;

    BACnetStatusFlags status_flags;
    BACnetEventState  event_state; // это свойство может отсуствовать

	//BACnetARRAY property_list; // BACnetARRAY[N] of BACnetPropertyIdentifier
	/*! таблица свойств - перечисление свойств

	массив
	{BACnetPropertyIdentifier, TYPE, (BacTemplate),  OFFSET(), ARRAY/LIST | WRITABLE},

	*/
};
struct _BACnetDevice {
    uint8_t  Protocol_Version;
    uint8_t  Protocol_Revision;
    uint32_t Protocol_Services_Supported[2];//!< BACnetBitString
    uint32_t Protocol_Object_Types_Supported[2];
    BACnetARRAY Object_List;            //!< BACnetARRAY of BACnetObjectIdentifier
    BACnetARRAY Configuration_Files;    //!< BACnetARRAY of BACnetObjectIdentifier
    BACnetLIST  Device_Address_Binding; //!< BACnetLIST of BACnetAddressBinding Фстройства на линии

    uint32_t Database_Revision; // параметр инкрементитс§, когда что-то мен§етс§ в базе данных
};


struct _BACnetPropertyReference {
    enum _BACnetPropertyIdentifier property_identifier;
    uint16_t property_array_index;
};
struct _BACnetObjectPropertyReference {
    BACnetObjectIdentifier object_identifier;
    enum _BACnetPropertyIdentifier property_identifier;// используютс€ 22 бита
    uint16_t property_array_index;// можно оставить только 10 бит, тогда уложитс€ в 32 бита
};
struct _BACnetDeviceObjectReference {
    BACnetObjectIdentifier device_identifier;
    BACnetObjectIdentifier object_identifier;
};
struct _BACnetDeviceObjectPropertyReference {
    BACnetObjectIdentifier device_identifier;
    BACnetObjectIdentifier object_identifier;
    enum _BACnetPropertyIdentifier property_identifier;// используютс€ 22 бита
    uint16_t property_array_index;// можно оставить только 10 бит, тогда уложитс€ в 32 бита
};
struct _BACnetDeviceObjectPropertyValue {
    BACnetObjectIdentifier device_identifier;
    BACnetObjectIdentifier object_identifier;
    enum _BACnetPropertyIdentifier property_identifier;// используютс€ 22 бита
    uint16_t property_array_index;// можно оставить только 10 бит, тогда уложитс€ в 32 бита
    BACnetValue property_value;// не уверен
};
struct _BACnetPropertyValue {
    enum _BACnetPropertyIdentifier property_identifier;
    uint16_t property_array_index;
    uint16_t  priority;
    BACnetValue property_value;// не уверен
};
struct _BACnetObjectPropertyValue {
    BACnetObjectIdentifier object_identifier;
    enum _BACnetPropertyIdentifier property_identifier;
    uint16_t property_array_index;
    uint16_t priority;
    BACnetValue property_value;// не уверен
};



//int bacnet_create_object(BACnetObjectIdentifier oid, GSList* list_of_initial_values);

// используетс€ в јPDU и _encode.c
typedef struct _BacContext BacContext;
struct _BacContext {
    uint32_t type;
	char* name;
    const BacContext* ref;
    uint16_t size;
};

/*!
    {(length<<16)|(context_id<<8)|tag}
*/
static inline
void bacnet_value_init(BACnetValue* node, uint32_t type) {
    uint32_t *node_type = (void*)node;
    *node_type = type;
}
static inline
void bacnet_value_copy (const BACnetValue *src_value, BACnetValue *dest_value) {
    *dest_value = *src_value;
}
void bacnet_value_unset(BACnetValue* node);
void bacnet_value_free (BACnetValue* node);
void bacnet_value_set (const BACnetValue* value, void* data, int size);
void bacnet_value_get (      BACnetValue* value, void* data, int size);
/* определить преобразование между типами данных */
int  bacnet_value_transform(BACnetValue* src_node, BACnetValue* dst_node);
int  bacnet_value_type_name(uint16_t from_type_id);
int  bacnet_value_type_compatible   (uint16_t from_type_id, uint16_t to_type_id);
int  bacnet_value_type_transformable(uint16_t from_type_id, uint16_t to_type_id);
void bacnet_value_register_transform(uint16_t from_type_id, uint16_t to_type_id);

void bacnet_node_debug (const BACnetValue* node, const BacContext* context, int size, int offset);
uint8_t* bacnet_node_decode(uint8_t* buf, BACnetValue* node, /*const BacContext* context,*/ uint32_t size);

/*! доступ к элементу списка с заданным идентификатором типа (контекстным номером) */
#if 0
static inline BACnetLIST * bacnet_node_get   (BACnetLIST * list, uint8_t context_id)
{
    while (list) {
        BACnetValue * node = list->value.node;
        if (node->type_id == context_id)
            break;
        list = list->next;
    }
    return list;
}
static inline BACnetLIST * bacnet_node_get_nth   (BACnetLIST * list, uint8_t context_id, unsigned int n)
{
    while (list) {
        BACnetValue * node = list->value.node;
        if (node->type_id == context_id && 0==n--)
            break;
        list = list->next;
    }
    return list;
}
static inline BACnetLIST * bacnet_node_append(BACnetLIST * top, BACnetValue* node)
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
#endif

/*! платформо зависимые функции */
uint64_t bacnet_get_timestamp();

#endif // BACnet_H
