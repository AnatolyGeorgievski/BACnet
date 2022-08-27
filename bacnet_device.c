/*! \defgroup 16_ 16 REMOTE DEVICE MANAGEMENT SERVICES
16.1.2 Service Procedure -- DeviceCommunicationControl
16.2.2 Service Procedure -- ConfirmedPrivateTransfer
16.3.2 Service Procedure -- UnconfirmedPrivateTransfer
16.4.2 Service Procedure -- ReinitializeDevice
16.5.2 Service Procedure -- ConfirmedTextMessage
16.6.2 Service Procedure -- UnconfirmedTextMessage
16.7.2 Service Procedure -- TimeSynchronization
16.8.2 Service Procedure -- UTCTimeSynchronization
16.9.2 Service Procedure -- Who-Has
16.9.4 Service Procedure -- I-Have
16.10.2 Service Procedure -- Who-Is
16.10.4 Service Procedure -- I-Am


*/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <cmsis_os.h>
#include <pthread_time.h>

//#include <pthread.h>
//#include <pthread_time.h>
#include "r3_tree.h"
#include "r3_slice.h"
#include "bacnet.h"
#include "bacnet_error.h"
#include "bacnet_net.h"
#include "bacnet_asn.h"
#include "bacnet_object.h"


enum _BACnetUnconfirmedServiceChoice {
i_am    =(0),
i_have  =(1),
unconfirmed_cov_notification   =(2),
unconfirmed_event_notification =(3),
unconfirmed_private_transfer   =(4),
unconfirmed_text_message       =(5),
time_synchronization =(6),
who_has =(7),
who_is  =(8),
utc_time_synchronization =(9),
write_group =(10),
unconfirmed_cov_notification_multiple =(11)
};

int Who_Is_req(Device_t* device, uint32_t device_instance_low_limit, uint32_t device_instance_high_limit);

//#define LIST_OF(type) struct _list_of_##type { struct _list_of_##type *next; type *value; } *
// описание сервисов

typedef struct _BACnetAddressBinding BACnetAddressBinding_t;
/*! The Device_Address_Binding property is a BACnetLIST of BACnetAddressBinding each of which consists of a BACnet
Object_Identifier of a Device object and a BACnet device address in the form of a BACnetAddress. Entries in the list
identify the actual device addresses that will be used when the remote device must be accessed via a BACnet service request.
A value of zero may be used for the network-number portion of BACnetAddress entries for other devices residing on the
same network as this device. The list may be empty if no device identifier-device address bindings are currently known to the
device. */
struct _BACnetAddressBinding {
    BACnetObjectIdentifier device_identifier;
    BACnetAddress_t device_address;
};


//void CONF_SERV_request -- пользователя
// void CONF_SERV_confirm -- это со стороны пользователя
void ABORT_req(Device_t *device, uint16_t err)
{
}
void REJECT_req(Device_t *device, uint16_t err)
{

}
void CONF_SERV_rsp(Device_t * device, uint8_t service_choice, uint8_t* data, size_t data_len)
{

}
void CONF_SERV_ind(Device_t * device, uint8_t service_choice, uint8_t* data, size_t data_len)
{
	//Device_t *device = dl->device;
	if (device->service[service_choice].indication) {
		BACnetValue* node = bacnet_value_list_new(service_choice);
		bacnet_node_decode(data, node, data_len);
		/* node = g_slice_new(BACnetValue);

		node->tag = ASN_CLASS_CONSTRUCTIVE|ASN_CLASS_CONTEXT;
		node->type_id = pdu.service_choice;
		node->value.list =NULL;*/
		BACnetAddr_t *source_addr = NULL;
		int result = device->service[service_choice].indication(device, source_addr, node);
		if (result<0) {// ошибка

		} else {
			data_len = bacnet_node_encode(data, node, data_len);
			CONF_SERV_rsp(device, service_choice, data, data_len);
		}
		bacnet_value_free(node);
	} else {// не поддерживается
		REJECT_req(device, REJECT_UNRECOGNIZED_SERVICE);
	}

}
void UNCONF_SERV_ind(Device_t * device, uint8_t service_choice, DataUnit_t* tr)//uint8_t* data, size_t data_len)
{

	// нужна ссылка на устройство от которого пришле запрос dl->destination_address (MAC) => net->destination_address (IP) => device_id
	//Device_t *device = dl->device;// это мой объект, который прилипляется (bind) к интерфейсу с MAC адресом \see datalink_bind(my_address, device);
	// \see Device_Address_Binding в структуре DeviceObject
	if (device->unconfirmed_service[service_choice].indication) {
		BACnetValue* node = bacnet_value_list_new(service_choice);
		bacnet_node_decode(tr->buffer, node, tr->size);
		/* node = g_slice_new(BACnetValue);

		node->tag = ASN_CLASS_CONSTRUCTIVE|ASN_CLASS_CONTEXT;
		node->type_id = pdu.service_choice;
		node->value.list =NULL;*/
		BACnetAddr_t* source_addr = tr->destination;
		device->unconfirmed_service[service_choice].indication(device, source_addr, node);
		bacnet_value_free(node);
	}
	DataUnitUnref(tr);
}


#if 0
/*! \brief обслуживание запросов к устройству */
int bacnet_device_service (Device_t *device)
{
	int result = 0;
	osService_t *self = device->dev_service;//osServiceGetId();
	osEvent* event  = osServiceGetEvent(self);
	/* событие выставляется на заврешение, обычно это событие osEventComplete,
	в случае службы на выходе должно быть osEventService и дополнительно тип события ожидания osEventSignal|osEventTimeout */
	if (event->status & osEventSignal) {
		uint32_t signals = event->value.signals;/* взять список сигналов, происходит атомарно */
		if (signals & UNCONF_SERV_indication) {// normal unconfirmed service
		} else
		if (signals & SEGMENT_ACK_indication) {
		} else
		if (signals & REJECT_indication) {
		} else
		if (signals & ABORT_indication) {
		}
	}
	return result;
}
#endif // 0
uint32_t * bacnet_value_serialize(BACnetValue* value, uint32_t*data)
{
	*data++ = *(uint32_t*)value;// вся шапка от value (tag, type, length)
	if (value->tag & ASN_CLASS_CONSTRUCTIVE) {// ожидаем список свойств, типа массив или
#if 1
		BACnetLIST* list = value->value.list;
		while (list) {
			data = bacnet_value_serialize(list->value.node, data);
			list = list->next;
		}
		// подправить поле length, по факту, длина поля, без выравнивания
#else // массив сохраняем
		BACnetValue *values = value->value.ptr;
		const int count = value->length;// длина или число элементов?
		int i;
		for (i=0;i<count; i++)
			data = bacnet_value_serialize(&values[i], data);
#endif
	} else {
		if(value->length > 4) {
			int i;
			int count = (value->length+3)>>2;
			uint32_t* src = value->value.ptr;
			for (i=0;i<count; i++)
				*data++ = *src++;
		} else
			*data++ = value->value.u;
	}
	return data;
}
void device_tree_serialize(uint32_t key, void* value, void ** buffer)
{
	uint32_t * data=*buffer;
	*data++ = key; // ObjectIdentifier или PropertyIdentifier
	data = bacnet_value_serialize((BACnetValue*) value, data);
}



BACnetValue* bacnet_value_load(uint32_t ** buffer)
{
	uint32_t* data = *buffer;
	BACnetValue *node = g_slice_alloc(sizeof(BACnetValue));
	*(uint32_t*)node = *data++;// вся шапка от value (tag, type, length)
	if (node->tag & ASN_CLASS_CONSTRUCTIVE) {// ожидаем список свойств, типа массив или
		const int count = node->length;// длина или число элементов?
		int i;
		BACnetLIST* list = NULL;
		for (i=0;i<count; i++) {
			BACnetLIST *elem = g_slice_alloc(sizeof(BACnetLIST));
			elem->value.node = bacnet_value_load(&data);
			elem->next = NULL;
			if(list) list->next = elem;
			else node->value.list = elem;
			list = elem;
		}
	} else {
		if(node->length > 4) {
			int count = (node->length+3)>>2;
#if 0
			uint32_t* dst = node->value.ptr = malloc(count<<2);
			int i;
			for (i=0;i<count; i++)
				*dst++ = *data++;
#else
			node->value.ptr =  data;
			data+=count;
#endif
		} else
			node->value.u = *data++;
	}
	*buffer = data;
	return node;
}

void device_tree_insert(Device_t* device, BACnetObjectIdentifier* oid, BACnetObject* object)
{
	tree_t * leaf = g_slice_alloc(sizeof(tree_t));
	tree_init(leaf, *(uint32_t*)oid, object);
	leaf = tree_insert_tree(&device->tree, leaf);
	if (leaf) {// если произошла замена объекта, то надо удалить предыдущий

	}
}
extern int bacnet_object_create (Device_t* device, BACnetObjectIdentifier *oid,  BACnetLIST* list_of_initial_values);
/*! \brief читаем список объектов от начала до конца сегмента */
void device_tree_init(Device_t* device, uint32_t *data, uint32_t *end)
{
	//tree_new();
	while (data<end) {
		BACnetObjectIdentifier *oid = (void*)data; // 22 бита -instance 10 бит -тип
		BACnetValue  * node = bacnet_value_load(&data);
		bacnet_object_create(device, oid, node->value.list);
		//device_tree_insert(device, (BACnetObjectIdentifier*) &key, object);
	}
}
/*!
    Этот сервис отвечает за запросы типа
*/
int device_discovery_service(Device_t *device)
{
#if 0
    Init:
        foreach(Local Network_t) {
            I_Am_req(_NPCI, device->instance.oid);
        }
#endif // 0
//    BACnetLIST* list = device->Device_Address_Binding;
//    BACnetAddressBinding_t *bindings = device->Device_Address_Binding;
    return 0;
}

/*!
    Этот сервис разыскивает связи.
*/
DeviceInfo_t* device_address_lookup(Device_t *device, uint32_t instance)
{
    //BACnetLIST* list = device->Device_Address_Binding;
    // remote_device->addr.virtual_address = instance;
    DeviceInfo_t * remote_device = tree_lookup(device->Device_Address_Binding, instance);// adress resolution
    if (remote_device==NULL) {
        // сначала затолкать объект, ждать пока его заполнит служба
        //remote_device =
        Who_Is_req(device, instance, instance+3);// может приписать таймаут
        // как дождаться ответа?
        //Who_Has_req(device, instance);
    }
    return remote_device;
}

/*! \defgroup _16_10 16.10 Who-Is and I-Am Services

The Who-Is service is used by a sending BACnet-user to determine the Device object identifier, the network address, or both,
of other BACnet devices that share the same internetwork. The Who-Is service is an unconfirmed service. The Who-Is
service may be used to determine the Device object identifier and network addresses of all devices on the network, or to
determine the network address of a specific device whose Device object identifier is known, but whose address is not. The IAm
service is also an unconfirmed service. The I-Am service is used to respond to Who-Is service requests. However, the IAm
service request may be issued at any time. It does not need to be preceded by the receipt of a Who-Is service request. In
particular, a device may wish to broadcast an I-Am service request when it powers up. The network address is derived either
from the MAC address associated with the I-Am service request, if the device issuing the request is on the local network, or
from the NPCI if the device is on a remote network.

    16.10.2 Service Procedure
If the receiving BACnet-user has a Slave_Proxy_Enable property and the Slave_Proxy_Enable for the receiving port is
TRUE, then the BACnet-user shall respond with an I-Am unconfirmed request for each of the slave devices on the MS/TP
network that are present in the Slave_Address_Binding property and that match the device range parameters. The I-Am
unconfirmed requests that are generated shall be generated as if the slave device originated the service. If the I-Am
unconfirmed request is to be placed onto the MS/TP network on which the slave device resides, then the MAC address
included in the packet shall be that of the slave device. In the case where the I-Am unconfirmed request is to be placed onto a
network other than that on which the slave device resides, then the network layer shall contain SLEN and SNET filled in with
the slave device's MAC address as if it were routing a packet originally generated by the slave device

16.10.4 Service Procedure
The sending BACnet-user shall broadcast or unicast the I-Am unconfirmed request. If the I-Am is broadcast, this broadcast
may be on the local network only, a remote network only, or globally on all networks at the discretion of the application. If
the I-Am is being sent in response to a previously received Who-Is, then the I-Am shall be sent in such a manner that the
BACnet-user that sent the Who-Is will receive the resulting I-Am. Since the request is unconfirmed, no further action is
required. A BACnet-user may issue an I-Am service request at any time.
*/
struct _Who_Is_Request{
    uint32_t low_limit; // [0] Unsigned (0..4194303) OPTIONAL, -- shall be used as a pair, see 16.10
    uint32_t high_limit;// [1] Unsigned (0..4194303) OPTIONAL -- shall be used as a pair, see 16.10
};
static const ParamSpec_t Who_Is_Request_desc [] = {
    {ASN_TYPE_UNSIGNED, OFFSET(struct _Who_Is_Request,low_limit),  RO},
    {ASN_TYPE_UNSIGNED, OFFSET(struct _Who_Is_Request,high_limit), RO},
};
typedef struct _I_Am_Request I_Am_Request_t;
struct _I_Am_Request {
    BACnetObjectIdentifier device_identifier;
    uint16_t max_apdu_length_accepted;
    uint16_t segmentation_supported;
    uint16_t vendor_id;
};
static const ParamSpec_t I_Am_Request_desc [] = {
    {ASN_TYPE_OID,      OFFSET(I_Am_Request_t,device_identifier),  RO},
    {ASN_TYPE_UNSIGNED, OFFSET(I_Am_Request_t,max_apdu_length_accepted), RO,2},
    {ASN_TYPE_ENUMERATED, OFFSET(I_Am_Request_t,segmentation_supported), RO,2},
    {ASN_TYPE_UNSIGNED, OFFSET(I_Am_Request_t,vendor_id), RO,2},// |RANGE(1, 16)
};
int I_Am_ind(Device_t* device, BACnetAddr_t * source_address, BACnetValue* node)
{
    BACnetLIST* list = node->value.list;
    I_Am_Request_t request;
    list = bacnet_value_list_parse(list, &request, DEF(I_Am_Request));
    bool new_device = false;
    printf("%s: add device address\n",__FUNCTION__);
    DeviceInfo_t* dev_info = tree_lookup(device->Device_Address_Binding, request.device_identifier.instance);// adress resolution

/// todo ограничить популяцию базы, нас могут интересовать далеко не все адреса
    if (dev_info==NULL) {
        dev_info = g_slice_alloc(sizeof(DeviceInfo_t));
        new_device = true;
        // предусмотреть сигнал для новой записи
    }
    dev_info->addr = *source_address;
    dev_info->max_apdu_length_accepted = request.max_apdu_length_accepted;
    dev_info->segmentation_supported = request.segmentation_supported;
    if (new_device){
        tree_t * elem = g_slice_alloc(sizeof(tree_t));
        elem = tree_init(elem, request.device_identifier.instance, dev_info);
        elem = tree_insert(&device->Device_Address_Binding, elem);
        if (elem) {// если по какой-то причине запись не пуста
            if (elem->value) g_slice_free1(sizeof(DeviceInfo_t), elem->value);
            g_slice_free1(sizeof(tree_t), elem);
        }
//        if (dev_info==NULL) osSignalSet(device->thread, 1);
    }

#if 0
    DeviceInfo_t *dev= g_slice_alloc(sizeof(DeviceInfo_t));
    BACnetAddr_t* source_addr;
    if (SA->len==1) {
        dev->network_number = SA->network_number;
        dev->mac_addr = SA->mac[0];
        dev->port_id = dl->port_id;
    } else {
        ttl_datalist_set(&dl->vmac_table, request.device_identifier.instance, SA);
        dev->mac_addr = 0;
        source_addr->port_id
    }



    tree_t * leaf = g_slice_alloc(sizeof(tree_t));
    tree_init(leaf, request.device_identifier.instance, dev);
    leaf = tree_insert(device->arp_table, leaf);
    if (leaf!=NULL) {
        // удалить DeviceInfo_t
        g_slice_free1(sizeof(tree_t), leaf);
    }
#endif // 0
    return 0;
}
int DataUnitWait(DataUnit_t* tr, uint32_t timeout_ms)
{
    struct timespec timeout;// = {0};
    uint32_t timestamp = osKernelSysTick();
    while (tr->refcount!=0) {
        timeout.tv_sec=0;
        timeout.tv_nsec=1000000;
        clock_nanosleep(CLOCK_MONOTONIC, 0, &timeout,NULL);
        uint32_t current = osKernelSysTick();
        if ((current - timestamp) >= timeout_ms*1000000) return 1;
    }
    return 0;
}

int I_Am_req  (Device_t* device)
{
    I_Am_Request_t request;
    request.device_identifier = device->instance.oid;
    request.vendor_id         = 0x9148;//device->vendor_identifier;
    request.max_apdu_length_accepted = device->protocol.max_apdu_length_accepted;
    request.segmentation_supported   = device->protocol.segmentation_supported;


    DataUnit_t* tr = g_slice_alloc(sizeof(DataUnit_t));//&device->apdu_requesting_transfer;// -- выделение из массива//g_slice_alloc(sizeof(DataUnit_t));//&device->apdu_transfer;//&device->transfer;// здесь трансфер рождается
    int res = DataUnitWait(tr, 2000);
    if (res) {
        printf("DataUnit Confirm timeout\n");
    }
    tr->buffer = malloc(512)+32;//device->apdu_requesting_buffer;//osMemoryPoolAlloc(device->buffer_pool, 2000);
    if (tr->buffer==NULL) return -1;// нет свободного буфера
    tr->size   = bacnet_paramspec_encode(tr->buffer, &request, DEF(I_Am_Request));
    tr->status = i_am;// enum _BACnetUnconfirmedServiceChoice
    tr->refcount = 0;// нужен!
    int i;
    for(i=0;i<tr->size; i++) {
        printf(" %02X", tr->buffer[i]);
    }
    printf(" -- out\n");
    UNCONF_SERV_req(device, /* destination_address */ NULL, tr);
//    dataunit_wait(tr, 2000);// ждем завершения транзацкции

    return 0;
}
int Who_Is_ind(Device_t* device, BACnetAddr_t * source_address, BACnetValue* node)
{
    BACnetLIST* list = node->value.list;
    struct _Who_Is_Request request;
    request.high_limit=UNDEFINED;
    request.low_limit=0;
    list = bacnet_value_list_parse(list, &request, DEF(Who_Is_Request));
    if ((request.low_limit <= device->instance.oid.instance)
    && (device->instance.oid.instance <= request.high_limit)) {
        I_Am_req(device);
    }
    return 0;
}
int Who_Is_req(Device_t* device, uint32_t device_instance_low_limit, uint32_t device_instance_high_limit)
{
    struct _Who_Is_Request request;
    request.low_limit = device_instance_low_limit;
    request.high_limit = device_instance_high_limit;

    DataUnit_t* tr = &device->apdu_requesting_transfer;
    int res = DataUnitWait(tr, 2000);
    if (res) {
        printf("DataUnit Confirm timeout\n");
    }
    tr->buffer = malloc(512)+20;//device->apdu_requesting_buffer;
    if (device_instance_low_limit==0 && device_instance_high_limit==UNDEFINED) {
        tr->size   = bacnet_paramspec_encode(tr->buffer, &request, DEF(I_Am_Request));
    } else
        tr->size = 0;
    tr->status = who_is;// enum _BACnetUnconfirmedServiceChoice
    UNCONF_SERV_req(device, /* destination_address */ NULL, tr);
    return 0;
}

typedef struct _PrivateTransfer_Request ConfirmedPrivateTransfer_Request_t;
struct _PrivateTransfer_Request {
    uint16_t vendor_id;
    uint16_t service_number;
    BACnetValue service_parameters;
};
typedef struct _ConfirmedPrivateTransfer_Ack     ConfirmedPrivateTransfer_Ack_t;
struct _ConfirmedPrivateTransfer_Ack {
    uint16_t vendor_id;
    uint16_t service_number;
    BACnetValue result_block;
};
typedef struct _PrivateTransfer_Request UnconfirmedPrivateTransfer_Request_t;
static const ParamSpec_t PrivateTransfer_Request_desc [] = {
    {ASN_TYPE_UNSIGNED, OFFSET(UnconfirmedPrivateTransfer_Request_t,vendor_id), RO,2},
    {ASN_TYPE_UNSIGNED, OFFSET(UnconfirmedPrivateTransfer_Request_t,service_number), RO,20},// |RANGE(1, 16)
};

typedef struct _ConfirmedTextMessage_Request ConfirmedTextMessage_Request_t;
struct _ConfirmedTextMessage_Request {
};
typedef struct _DeviceCommunicationControl_Request DeviceCommunicationControl_Request_t;
struct _DeviceCommunicationControl_Request {
};
typedef struct _ReinitializeDevice_Request ReinitializeDevice_Request_t;
enum {// reinitialized_state_of_device
    coldstart       =(0),
    warmstart       =(1),
    start_backup    =(2),
    end_backup      =(3),
    start_restore   =(4),
    end_restore     =(5),
    abort_restore   =(6),
    activate_changes=(7)
};
struct _ReinitializeDevice_Request {
    uint8_t reinitialized_state_of_device;
    CharacterString password;
};
static const ParamSpec_t ReinitializeDevice_Request_desc [] = {
    {ASN_TYPE_ENUMERATED, OFFSET(ReinitializeDevice_Request_t,reinitialized_state_of_device), RO,1},
    {ASN_TYPE_STRING, OFFSET(ReinitializeDevice_Request_t,password), RO,20},// |RANGE(1, 16)
};

typedef struct _TimeSynchronization_Request TimeSynchronization_Request_t;
typedef struct _TimeSynchronization_Request UTCTimeSynchronization_Request_t;
struct _TimeSynchronization_Request {
    BACnetDateTime_t time;
};

int bacnet_device_service(void* service_data);

#define DEVICE_NUM_PORTS 4
static void device_services_init(Device_t *device);
Device_t* bacnet_device_init(uint8_t* config_image)
{
    Device_t *device = malloc(sizeof(Device_t));
    device->instance.oid.type = BACnetObjectType_DEVICE;
    device->instance.oid.instance = 12;
    device->vendor_identifier=0;
    device->protocol.max_apdu_length_accepted=480;
    device->protocol.segmentation_supported=0;

    device->apdu_requesting_buffer = malloc(device->protocol.max_apdu_length_accepted+32)+20;// +32
	osAsyncQueueInit(&device->apdu_requesting_queue);

    device->apdu_requesting_transfer.buffer = malloc(512)+20;
    device->apdu_requesting_transfer.size=0;
    device->apdu_requesting_transfer.status=0;
    device->apdu_requesting_transfer.refcount=0;
    device->apdu_requesting_transfer.destination=NULL;

    device->apdu_transfer.buffer = malloc(512)+20;
    device->apdu_transfer.size=0;
    device->apdu_transfer.status=0;
    device->apdu_transfer.refcount=0;
    device->apdu_transfer.destination=NULL;
    device->num_ports = DEVICE_NUM_PORTS;
    device->network_ports = malloc(sizeof(void*) * DEVICE_NUM_PORTS);
    int i;
    for (i=0; i< DEVICE_NUM_PORTS;i++)
        device->network_ports[i] = NULL;

    device_services_init(device);
    device->Device_Address_Binding=NULL;
    // число портов устройства?
//    device->port_number;
//    device->ports = malloc(sizeof(void*)*device->port_number);

    //_init(&device->apdu_transfer.mutex, NULL);
//    device->dev_service = osServiceCreate(NULL, bacnet_device_service, device);
//    osServiceTimerStart(device->dev_service, 2000);
//    I_Am_req(device);
//    osServiceRun(device->dev_service);
    return device;
}
/*! \brief порт отдается устройству */
void bacnet_device_port(Device_t* device, int port_id, DataLink_t* datalink)
{
    device->network_ports[port_id] = datalink;
    datalink->device = device;
}
/*!

По процедурным вопросам
*/
/*
int bacnet_device_service(void* service_data)
{
    Device_t* device = service_data;
    osService_t * self = device->dev_service;
    osEvent *event = osServiceGetEvent(self);

    if (event->status & osEventTimeout) {
        osServiceTimerStop(self);
        printf("TIMEOUT! Run I_Am service\n");
        I_Am_req(device);
    } else
    if (event->status & osEventSignal) {
        uint32_t signals = osServiceGetSignals(self);
        if (signals & N_REPORT_indication){// изменилось состояние интерфейса?
            I_Am_req(device);
        }
    }
    return 0;
}
*/
int bacnet_device_test(Device_t* device)
{
    printf("TEST!! \n");
    I_Am_req(device);
sleep(1);
    Who_Is_req(device,0,UNDEFINED);
    sleep(10);
    return 0;
}
static BACnetService_t  unconfirmed_services[] = {
[i_am]  = {.indication= I_Am_ind},
[who_is] = {.indication= Who_Is_ind},
[who_has] = {.indication= NULL},
};
static void device_services_init(Device_t *device)
{
    device->unconfirmed_service = unconfirmed_services;
}
