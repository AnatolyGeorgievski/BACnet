// BACnetEngineeringUnits
#ifndef BACnet_ERRORS
#define BACnet_ERRORS

enum _BACnetErrorClass {
DEVICE = (0),
OBJECT = (1),
PROPERTY = (2),
RESOURCES = (3),
SECURITY = (4),
SERVICES = (5),
VT = (6),
COMMUNICATION = (7),
};
enum _BACnetErrorCode {
ABORT_APDU_TOO_LONG = (123),
ABORT_APPLICATION_EXCEEDED_REPLY_TIME = (124),
ABORT_BUFFER_OVERFLOW = (51),
ABORT_INSUFFICIENT_SECURITY = (135),
ABORT_INVALID_APDU_IN_THIS_STATE = (52),
ABORT_OTHER = (56),
ABORT_OUT_OF_RESOURCES = (125),
ABORT_PREEMPTED_BY_HIGHER_PRIORITY_TASK = (53),
ABORT_PROPRIETARY = (55),
ABORT_SECURITY_ERROR = (136),
ABORT_SEGMENTATION_NOT_SUPPORTED = (54),
ABORT_TSM_TIMEOUT = (126),
ABORT_WINDOW_SIZE_OUT_OF_RANGE = (127),
ACCESS_DENIED = (85),
ADDRESSING_ERROR = (115),
BAD_DESTINATION_ADDRESS = (86),
BAD_DESTINATION_DEVICE_ID = (87),
BAD_SIGNATURE = (88),
BAD_SOURCE_ADDRESS = (89),
BAD_TIMESTAMP = (90),
BUSY = (82),
CANNOT_USE_KEY = (91),
CANNOT_VERIFY_MESSAGE_ID = (92),
CHARACTER_SET_NOT_SUPPORTED = (41),
COMMUNICATION_DISABLED = (83),
CONFIGURATION_IN_PROGRESS = (2),
CORRECT_KEY_REVISION = (93),
COV_SUBSCRIPTION_FAILED = (43),
DATATYPE_NOT_SUPPORTED = (47),
DELETE_FDT_ENTRY_FAILED = (120),
DESTINATION_DEVICE_ID_REQUIRED = (94),
DEVICE_BUSY = (3),
DISTRIBUTE_BROADCAST_FAILED = (121),
DUPLICATE_ENTRY = (137),
DUPLICATE_MESSAGE = (95),
DUPLICATE_NAME = (48),
DUPLICATE_OBJECT_ID = (49),
DYNAMIC_CREATION_NOT_SUPPORTED = (4),
ENCRYPTION_NOT_CONFIGURED = (96),
ENCRYPTION_REQUIRED = (97),
FILE_ACCESS_DENIED = (5),
FILE_FULL = (128),
INCONSISTENT_CONFIGURATION = (129),
INCONSISTENT_OBJECT_TYPE = (130),
INCONSISTENT_PARAMETERS = (7),
INCONSISTENT_SELECTION_CRITERION = (8),
INCORRECT_KEY = (98),
INTERNAL_ERROR = (131),
INVALID_ARRAY_INDEX = (42),
INVALID_CONFIGURATION_DATA = (46),
INVALID_DATA_TYPE = (9),
INVALID_EVENT_STATE = (73),
INVALID_FILE_ACCESS_METHOD = (10),
INVALID_FILE_START_POSITION = (11),
INVALID_KEY_DATA = (99),
INVALID_PARAMETER_DATA_TYPE = (13),
INVALID_TAG = (57),
INVALID_TIMESTAMP = (14),
INVALID_VALUE_IN_THIS_STATE = (138),
KEY_UPDATE_IN_PROGRESS = (100),
LIST_ELEMENT_NOT_FOUND = (81),
LOG_BUFFER_FULL = (75),
LOGGED_VALUE_PURGED = (76),
MALFORMED_MESSAGE = (101),
MESSAGE_TOO_LONG = (113),
MISSING_REQUIRED_PARAMETER = (16),
NETWORK_DOWN = (58),
NO_ALARM_CONFIGURED = (74),
NO_OBJECTS_OF_SPECIFIED_TYPE = (17),
NO_PROPERTY_SPECIFIED = (77),
NO_SPACE_FOR_OBJECT = (18),
NO_SPACE_TO_ADD_LIST_ELEMENT = (19),
NO_SPACE_TO_WRITE_PROPERTY = (20),
NO_VT_SESSIONS_AVAILABLE = (21),
NOT_CONFIGURED = (132),
NOT_CONFIGURED_FOR_TRIGGERED_LOGGING = (78),
NOT_COV_PROPERTY = (44),
NOT_KEY_SERVER = (102),
NOT_ROUTER_TO_DNET = (110),
OBJECT_DELETION_NOT_PERMITTED = (23),
OBJECT_IDENTIFIER_ALREADY_EXISTS = (24),
OPERATIONAL_PROBLEM = (25),
OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED = (45),
OTHER = (0),
OUT_OF_MEMORY = (133),
PARAMETER_OUT_OF_RANGE = (80),
PASSWORD_FAILURE = (26),
PROPERTY_IS_NOT_A_LIST = (22),
PROPERTY_IS_NOT_AN_ARRAY = (50),
READ_ACCESS_DENIED = (27),
READ_BDT_FAILED = (117),
READ_FDT_FAILED = (119),
REGISTER_FOREIGN_DEVICE_FAILED = (118),
REJECT_BUFFER_OVERFLOW = (59),
REJECT_INCONSISTENT_PARAMETERS = (60),
REJECT_INVALID_PARAMETER_DATA_TYPE = (61),
REJECT_INVALID_TAG = (62),
REJECT_MISSING_REQUIRED_PARAMETER = (63),
REJECT_OTHER = (69),
REJECT_PARAMETER_OUT_OF_RANGE = (64),
REJECT_PROPRIETARY = (68),
REJECT_TOO_MANY_ARGUMENTS = (65),
REJECT_UNDEFINED_ENUMERATION = (66),
REJECT_UNRECOGNIZED_SERVICE = (67),
ROUTER_BUSY = (111),
SECURITY_ERROR = (114),
SECURITY_NOT_CONFIGURED = (103),
SERVICE_REQUEST_DENIED = (29),
SOURCE_SECURITY_REQUIRED = (104),
_SUCCESS = (84),
TIMEOUT = (30),
TOO_MANY_KEYS = (105),
UNKNOWN_AUTHENTICATION_TYPE = (106),
UNKNOWN_DEVICE = (70),
UNKNOWN_FILE_SIZE = (122),
UNKNOWN_KEY = (107),
UNKNOWN_KEY_REVISION = (108),
UNKNOWN_NETWORK_MESSAGE = (112),
UNKNOWN_OBJECT = (31),
UNKNOWN_PROPERTY = (32),
UNKNOWN_ROUTE = (71),
UNKNOWN_SOURCE_MESSAGE = (109),
UNKNOWN_SUBSCRIPTION = (79),
UNKNOWN_VT_CLASS = (34),
UNKNOWN_VT_SESSION = (35),
UNSUPPORTED_OBJECT_TYPE = (36),
VALUE_NOT_INITIALIZED = (72),
VALUE_OUT_OF_RANGE = (37),
VALUE_TOO_LONG = (134),
VT_SESSION_ALREADY_CLOSED = (38),
VT_SESSION_TERMINATION_FAILURE = (39),
WRITE_ACCESS_DENIED = (40),
WRITE_BDT_FAILED = (116)
};
#define ERROR(errClass, errCode) ~((errClass<<16)|errCode)
#endif // BACnet_ERRORS