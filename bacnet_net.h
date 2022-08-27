#ifndef BACnetNETWORK_H
#define BACnetNETWORK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"
#include "r3_object.h"
#include "r3_tree.h"

#define PORT_BACNET_IP 0xBAC0 // "47808"
#define GLOBAL_BROADCAST 0xFFFF
#define LOCAL_NETWORK 0x0
#define BVLL_TYPE_BACNET_IP    0x81
#define BVLL_TYPE_BACNET_IPv6  0x82
#define BVLL_TYPE_RADIAN	155

/*! 6.2.2 Network Layer Protocol Control Information */

/*! Bit 2: The value of this bit corresponds to the data_expecting_reply parameter in the N-UNITDATA primitives */
#define NPCI_DATA_EXPECTING_REPLY 0x04
/*! Bits 1,0: Network priority where:
    B'11' = Life Safety message
    B'10' = Critical Equipment message
    B'01' = Urgent message
    B'00' = Normal message
*/
#define NPCI_Life_Safety 0x03
#define NPCI_Critical   0x02
#define NPCI_Urgent     0x01
#define NPCI_Normal     0x00



typedef struct _BACnetAddr BACnetAddr_t;
struct _BACnetAddr {
    union {
        uint8_t mac[sizeof(void*)];// IPv4 использует 4байта +порты=2байта
        uint32_t vmac_addr;// VMAC -- device ID в 22 бита,
        uint32_t in_addr;
        uint8_t *ipv6_addr;// MAC -- это способ хранения даресов IPv6,
    };
    uint16_t in_port;

    uint16_t network_number;//!< A value of 0 indicates the local network
	uint8_t len;
};
//структура используется для хранения адреса и номера порта в IPv4
typedef struct _BACnetHostNPort BACnetHostNPort_t;
struct _BACnetHostNPort {
    union {
        uint8_t         mac[sizeof(void*)];
        uint32_t        vmac_addr;
        uint32_t        in_addr;
        uint8_t *ipv6_addr;
    };
    uint16_t in_port;
};
typedef struct _DeviceObject Device_t;
typedef struct _DeviceInfo DeviceInfo_t;
typedef struct _NetworkPortObject NetworkPort_t;
typedef struct _BACnetNPCI BACnetNPCI;// \see ICI (Interface Control Info)
struct _BACnetNPCI {

    BACnetAddr_t DA;
    BACnetAddr_t SA;

    uint8_t  version;// 0x01
    uint8_t  control;// data_expecting_reply|priority
    uint8_t  hop_count;// нормальная практика = 15 или 255
//    uint8_t  message_type;
//    uint16_t vendor_id;

//    uint8_t next_router;

//	uint8_t data_expecting_reply:1;
	uint8_t await_response:1;
	uint8_t segmentation_supported:1;
    struct {
		uint8_t *buffer;
		uint16_t length;
		uint16_t MTU;
		int32_t result;

	} response;
	DeviceInfo_t *local;//!< устройство с нашей стороны
	DeviceInfo_t *peer;//!< устройство с которым налаживаем общение
	osService_t* svc;// сервис который принимает сигналы тип CONF_SERV_indication CONF_SERV_confirm ABORT_indication
	void* arg;

//	osSignalRef_t signal;// пришпоривает службу
};

typedef struct _BACnetAPDU BACnetAPDU;
struct _BACnetAPDU {
    uint8_t type;// SEG|MOR|NAK|SRV
    uint8_t invoke_id;
    uint8_t/* enum _BACnetServicesSupported */ service_choice;
	uint8_t max_response;
    uint8_t seq_number;
    uint8_t window_size;
};

struct _DL_hdr {// даталинк объединяет сущности, npci
	BACnetNPCI npci;
	BACnetAPDU apdu;
	uint16_t   network_number;
	osThreadId owner;
	void* handler;// драйвер RS-485
	
	uint8_t TS;// свой адрес
	uint8_t FrameType;
	uint8_t source_address;
	uint8_t destination_address;
//	uint8_t priority:4; -- \sa npdu->control
	uint8_t state:1; // IDLE | ANSWER_DATA_REQUEST
	uint8_t master:1;
	uint8_t data_expecting_reply:1;
	uint8_t *data;
	size_t   data_len;

};
struct _DeviceInfo {
    struct _DeviceObject * device; // сам объект
	tree_t* device_objects;

    BACnetAddr_t addr;
    enum {ST_INIT, ST_FAIL, ST_TIMEOUT, ST_STARTUP, ST_DONE, ST_UPLOAD} discovery_state;
};

typedef uint8_t IPv6_Addr_t __attribute__((vector_size(16)));

typedef struct _NetworkRoutingInfo NetworkRoutingInfo_t;
typedef struct _DataLink_IPv6 DataLink_IPv6_t;
typedef struct _DataLink_IPv4 DataLink_IPv4_t;
typedef struct _DataLink DataLink_t;
//typedef struct _DataUnit DataUnit_t;
typedef struct _FDT BlackList_t;
typedef struct _FDT FDT_t;
typedef struct _BDT BDT_t;

struct _DataLink {
   	osAsyncQueue_t queue;// очередь может быть свойством службы или даталинка
    uint16_t network_number;
    int (*UNITDATA_request)(struct _DataLink *dl, const BACnetAddr_t* DA, uint8_t* data, size_t data_len);
    //Device_t* device;// устройство которому принадлежит линк
    DeviceInfo_t* local;
#if __linux__
    GMutex mutex;
#endif
};
typedef struct _DataUnit DataUnit_t;
#if 0 // усохло
struct _DataUnit {
    GCond condition;// используется только адрес GCond
    volatile int status;
    uint32_t device_id;

    uint8_t *buffer;
    uint16_t size;
    uint16_t serial_id;//!< идентификатор команды назначается сервисом
    BACnetHostNPort_t dest_addr;
};
#endif
struct _DataLink_IPv4 {
    struct _DataLink instance;
    BlackList_t *blacklist;// это всех касается, от этого зависит выживаемость системы
    int sock_unicast;// сокет для мультикастов и броадкастов в том случае когда порт не совпадает
    int sock_multicast;// сокет для мультикастов и броадкастов в том случае когда порт не совпадает
    BACnetHostNPort_t FD_BBMD_address;
    uint16_t subscription_lifetime;// sec, используется устройство типа Foreign Device при подписке
    enum {IP_Mode_NORMAL, IP_Mode_FOREIGN, IP_Mode_BBMD} mode:8;// режим работы устройства NORMAL FOREIGN BBMD;
    bool NAT_traversal;// логический признак по которому устройство применяет глобальный адрес вместо локального и ведет себя как
    bool BBMD_Accept_FD_Registrations;
    BDT_t *BDT;
    FDT_t *FDT;
    BACnetHostNPort_t global_address;// мой глобальный адрес
    BACnetHostNPort_t local_address;// мой локальный адрес

};
struct _DataLink_IPv6 {

    enum {IPv6_Mode_NORMAL, IPv6_Mode_FOREIGN, IPv6_Mode_BBMD} mode;
    bool BBMD_Accept_FD_Registrations;
    BDT_t *BDT;
    FDT_t *FDT;
    struct {
        IPv6_Addr_t in6_addr;
        uint16_t in_port;
    } FD_BBMD_Address; // этот же адрес используется для broadcast, BACnet_IPv6_Multicast_Address
    uint16_t FD_Subscription_Lifetime;
    uint32_t vmac_address;// 22 бита+1
    ttl_datalist_t vmac_table;
};
/*
struct _DeviceObject {
    struct _Object instance;// наследование объектов
    struct _DeviceInfo info;
    NetworkPort_t* network_ports;
    tree_t* tree;// дерево объектов
};
*/
typedef struct _BACnetRouterEntry BACnetRouterEntry_t;
struct _BACnetRouterEntry {
    struct _BACnetRouterEntry * next;
    BACnetAddr_t DA;
//    uint16_t network_number;
//    struct _BACnetHostNPort next_router;
//    BACnet next_router;// рутегие пока поддержим только на MSTP
//    DataLink_t* datalink;
};
enum _BACnetNetworkType {
    ETHERNET =(0),
    ARCNET =(1),
    MSTP =(2),
    PTP =(3),
    LONTALK =(4),
    IPv4 =(5),
    ZIGBEE =(6),
    VIRTUAL =(7),
    //-- formerly: non-bacnet (8), removed in version 1 revision 18
    IPv6 =(9),
    SERIAL =(10),
// ...
};
/*
struct _NetworkPort {
    struct _NetworkPort* next;
    enum _BACnetNetworkType network_type:8;
    BACnetRouterEntry_t* routing_table;
    DataLink_t* datalink;
};

*/

enum _BACnet_PDU {
	BACnet_Confirmed_Request_PDU 	=0x00,
	BACnet_Unconfirmed_Request_PDU	=0x10,
	BACnet_SimpleACK_PDU 			=0x20,
	BACnet_ComplexACK_PDU 			=0x30,
	BACnet_SegmentACK_PDU 			=0x40,
	BACnet_Error_PDU 				=0x50,
	BACnet_Reject_PDU 				=0x60,
	BACnet_Abort_PDU 				=0x70,
};

enum _BACnetConfirmedServiceChoice /* ENUMERATED */{
// -- Alarm and Event Services
ACKNOWLEDGE_ALARM =(0),
CONFIRMED_COV_NOTIFICATION =(1),
CONFIRMED_COV_NOTIFICATION_MULTIPLE =(31),
CONFIRMED_EVENT_NOTIFICATION =(2),
GET_ALARM_SUMMARY =(3),
GET_ENROLLMENT_SUMMARY =(4),
GET_EVENT_INFORMATION =(29),
LIFE_SAFETY_OPERATION =(27),
SUBSCRIBE_COV =(5),
SUBSCRIBE_COV_PROPERTY =(28),
SUBSCRIBE_COV_PROPERTY_MULTIPLE =(30),
// -- File Access Services
ATOMIC_READ_FILE  =(6),
ATOMIC_WRITE_FILE =(7),
// -- Object Access Services
ADD_LIST_ELEMENT    =(8),
REMOVE_LIST_ELEMENT =(9),
CREATE_OBJECT =(10),
DELETE_OBJECT =(11),
READ_PROPERTY =(12),
READ_PROPERTY_MULTIPLE =(14),
READ_RANGE =(26),
WRITE_PROPERTY =(15),
WRITE_PROPERTY_MULTIPLE =(16),
// -- Remote Device Management Services
DEVICE_COMMUNICATION_CONTROL =(17),
CONFIRMED_PRIVATE_TRANSFER =(18),
CONFIRMED_TEXT_MESSAGE =(19),
REINITIALIZE_DEVICE =(20),
// -- Virtual Terminal Services
VT_OPEN  =(21),
VT_CLOSE =(22),
VT_DATA  =(23)
};

enum _BACnetUnconfirmedServiceChoice /* ENUMERATED */{
I_AM =(0),
I_HAVE =(1),
UNCONFIRMED_COV_NOTIFICATION =(2),
UNCONFIRMED_EVENT_NOTIFICATION =(3),
UNCONFIRMED_PRIVATE_TRANSFER =(4),
UNCONFIRMED_TEXT_MESSAGE =(5),
TIME_SYNCHRONIZATION =(6),
WHO_HAS =(7),
WHO_IS =(8),
UTC_TIME_SYNCHRONIZATION =(9),
WRITE_GROUP =(10),
UNCONFIRMED_COV_NOTIFICATION_MULTIPLE =(11),
/// 135-2016bz
WHO_AM_I = (12),
YOU_ARE = (13)
};
enum _NetworkMessage {// Message type field
	MSG_Who_Is_Router_To_Network 	= 0x00,
	MSG_I_Am_Router_To_Network 		= 0x01,
	MSG_I_Could_Be_Router_To_Network= 0x02,
	MSG_Reject_Message_To_Network 	= 0x03,
	MSG_Router_Busy_To_Network 		= 0x04,
	MSG_Router_Available_To_Network = 0x05,
	MSG_Initialize_Routing_Table 	= 0x06,
	MSG_Initialize_Routing_Table_Ack= 0x07,
	MSG_Establish_Connection_To_Network = 0x08,
	MSG_Disconnect_Connection_To_Network= 0x09,
	MSG_Challenge_Request 	= 0x0A,
	MSG_Security_Payload 	= 0x0B,
	MSG_Security_Response 	= 0x0C,
	MSG_Request_Key_Update 	= 0x0D,
	MSG_Update_Key_Set 		= 0x0E,
	MSG_Update_Distribution_Key = 0x0F,
	MSG_Request_Master_Key 	= 0x10,
	MSG_Set_Master_Key 		= 0x11,
	MSG_What_Is_Network_Number = 0x12,
	MSG_Network_Number_Is 	= 0x13,
	// X'14' to X'7F': Reserved for use by ASHRAE
	// X'80' to X'FF': Available for vendor proprietary messages
};
enum _IPv6_BVLL_functions {// \see Table U-1. B/IPv6 BVLL Messages
    BVLL_Result                 = 0x00,
    BVLL_Original_Unicast_NPDU  = 0x01,
    BVLL_Original_Broadcast_NPDU= 0x02,
    BVLL_Address_Resolution     = 0x03,
    BVLL_Forwarded_Address_Resolution = 0x04,
    BVLL_Address_Resolution_ACK = 0x05,
    BVLL_Virtual_Address_Resolution = 0x06,
    BVLL_Virtual_Address_Resolution_ACK = 0x07,
    BVLL_Forwarded_NPDU         = 0x08,
    BVLL_Register_Foreign_Device= 0x09,
    BVLL_Delete_Foreign_Device_Table_Entry = 0x0A,
    BVLL_Secure_BVLL            = 0x0B,
    BVLL_Distribute_Broadcast_To_Network = 0x0C,
};
enum _IP_BVLC_functions {
    BVLC_Result = 0x00,
    BVLC_Write_Broadcast_Distribution_Table = 0x01,
    BVLC_Read_Broadcast_Distribution_Table  = 0x02,
    BVLC_Read_Broadcast_Distribution_Table_Ack = 0x03,
    BVLC_Forwarded_NPDU = 0x04,
    BVLC_Register_Foreign_Device = 0x05,
    BVLC_Read_Foreign_Device_Table = 0x06,
    BVLC_Read_Foreign_Device_Table_Ack = 0x07,
    BVLC_Delete_Foreign_Device_Table_Entry = 0x08,
    BVLC_Distribute_Broadcast_To_Network = 0x09,
    BVLC_Original_Unicast_NPDU =0x0a,   // Function: Original-Unicast-NPDU (0x0a)
    BVLC_Original_Broadcast_NPDU =0x0b, // Function: Original-Broadcast-NPDU (0x0b)
    BVLC_Secure_BVLL = 0x0c,
};

enum {// выделяем сигналы исходя из того что это может быть один тред, номера сигналов не должны пересекаться
	UNCONF_SERV_indication	=(1<<0),
	UNCONF_SERV_request		=(1<<1),
	CONF_SERV_request		=(1<<2),
	CONF_SERV_response 		=(1<<3),
	CONF_SERV_confirm 		=(1<<3),
	SEGMENT_ACK_indication	=(1<<4),
	REJECT_indication		=(1<<5),
	N_UNITDATA_indication 	=(1<<6),
	N_UNITDATA_request 		=(1<<7),
	N_REPORT_indication		=(1<<8),
	N_RELEASE_request		=(1<<9),
	DL_CONNECT_indication	=(1<<10),
	DL_CONNECT_request		=(1<<11),
	DL_CONNECT_confirm		=(1<<12),
	DL_DISCONNECT_indication=(1<<13),
	DL_DISCONNECT_request	=(1<<14),
	DL_DISCONNECT_confirm	=(1<<15),
	DL_UNITDATA_indication	=(1<<16),
	DL_UNITDATA_request		=(1<<17),
	DL_UNITDATA_confirm		=(1<<17),
	DL_REPORT_indication	=(1<<18),
	DL_RELEASE_request		=(1<<18),
	ABORT_indication		=(1<<21),
	ABORT_request			=(1<<22),
	SEC_ERR_indication      =(1<<24),
	COV_notification		=(1<<25),
};

/* расчет контрольных сумм для пакетов ms/tp */
uint8_t	 bacnet_crc8 (const uint8_t * buffer);
uint16_t bacnet_crc16(const uint8_t * buffer, int data_len);
size_t  cobs_decode(uint8_t * dst, uint8_t * src, size_t length);
size_t  cobs_encode(uint8_t * dst, uint8_t * src, size_t length);
int     cobs_crc32k_check(uint8_t * data, size_t length);
size_t  cobs_crc32k(uint8_t * data, size_t length);

int bacnet_ip_address(BACnetHostNPort_t* bac_addr, const char* address, uint16_t port);
//int bacnet_network_message_req(DataLink_t*dl, BACnetNPCI *npci, DataUnit_t* tr);
int bacnet_network_message(DataLink_t* dl, BACnetNPCI *npci,  uint8_t * buffer, size_t size /*DataUnit_t* tr*/);
int bacnet_relay(BACnetNPCI *npci,uint8_t * buffer, size_t size );
// \todo знак в size используется внутри
int bacnet_apdu_service(DataLink_t*dl, BACnetNPCI *npci, uint8_t * buffer, size_t size/* DataUnit_t* */);
int bacnet_bvlc_indication(DataLink_IPv4_t* dl, BACnetAddr_t *peer, uint8_t *buffer, size_t size);
int bacnet_bvll_indication(DataLink_t* dl, BACnetAddr_t *peer, uint8_t *buffer, size_t size);
int bacnet_ipv6_sendto(DataLink_t* dl, const BACnetAddr_t* destination_addr, const uint8_t *buffer, size_t size);
int bacnet_ip_sendto(DataLink_IPv4_t* dl, const BACnetHostNPort_t* destination_addr, uint8_t *buffer, size_t size);
//int bacnet_datalink_indication(DataLink_t*dl, BACnetAddr_t *peer, uint8_t *buf, size_t size);
//int bacnet_datalink_request (DataLink_t* dl, BACnetNPCI* npci, uint8_t *buffer, size_t size);
int bacnet_datalink_indication(DataLink_t* dl, BACnetAddr_t * peer, uint8_t *buffer, size_t size/* , bool data_expecting_reply*/);
void bacnet_datalink_init(DataLink_t* dl, uint16_t network_number, DeviceInfo_t* local_device);
int bacnet_network_message_req(NetworkPort_t * network_ports, uint32_t message_type, uint8_t* buffer, size_t size);
/*! \brief Создает широковещательный адрес сети BACnet для данного интерфейса
    \todo datalink->broadcast - содержит ссылку IPv6 (128bit)
 */
#if 0
static inline void bacnet_local_broadcast1(DataLink_t* dl, BACnetAddr_t* addr)
{
    addr->network_number = 0;//dl->network_number; -- 0- для данной сети
    addr->len=0;
}
#endif

// от службы, к сети ISO TR 8509
int   CONF_SERV_req(BACnetNPCI *npci, uint8_t  service_choice, uint8_t * apdu_buffer, size_t apdu_len);
void UNCONF_SERV_req(BACnetNPCI *npci, uint8_t  service_choice, uint8_t * apdu_buffer, size_t apdu_len);
void SEGMENT_ACK_req();
void      REJECT_req(Device_t *dl, uint16_t err);
//void       ABORT_req();
// от сети к службе
void UNCONF_SERV_ind(BACnetNPCI *npci, uint8_t  service_choice, uint8_t * apdu_buffer, size_t apdu_len);
int    CONF_SERV_ind(BACnetNPCI *npci, uint8_t  service_choice, uint8_t * apdu_buffer, size_t apdu_len);
void SEGMENT_ACK_ind();
void      REJECT_ind(Device_t *dl, uint16_t err);
void       ABORT_ind(Device_t *dl, uint16_t err);
void     SEC_ERR_ind();
// от приложения к службе
//void   CONF_SERV_rsp();
void   CONF_SERV_cnf();

// от сети
void  N_UNITDATA_ind(Device_t *dl, uint8_t pdu, uint8_t* data, size_t data_len);
void   N_RELEASE_req(Device_t *dl);
void    N_REPORT_ind(Device_t *dl, uint8_t err_condition,  uint16_t error_parameters);


/* This primitive is passed from the MS/TP entity to the network layer to indicate the arrival of an NPDU from the specified remote entity */
//void DL_UNITDATA_req (struct _DL_hdr * hdr, uint8_t* data, size_t data_len);
/* */
//void DL_UNITDATA_ind (struct _DL_hdr * hdr, uint8_t* data, size_t data_len);
/* This primitive is generated from the network layer to the MS/TP entity to indicate that no reply is available from the higher layers.*/
//void DL_RELEASE_req  (void);



#endif // BACnetNETWORK_H
