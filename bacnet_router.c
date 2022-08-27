/*! 6.6 BACnet Routers
BACnet routers are devices that interconnect two or more BACnet networks to form a BACnet internetwork. BACnet routers
shall, at a minimum, implement the device requirements as specified in Clause 22.1.5. Table 6-1 specifies the maximum
NPDU length of the different data link layer types. Routers shall be capable of routing the maximum sized NPDUs between
any two of those data link layers supported by the router based on the destination data link maximum NPDU size. BACnet
routers make use of BACnet network layer protocol messages to maintain their routing tables. */

/*! 6.6.1 Routing Tables
By definition, a router is a device that is connected to at least two BACnet networks. Each attachment is through a "port." A
"routing table" consists of the following information for each port:

(a) the MAC address of the port's connection to its network;
(b) the 2-octet network number of the directly connected network;
(c) a list of network numbers reachable through the port along with the MAC address of the next router on the path to each
network number and the reachability status of each such network.

The "reachability status" is an implementation-dependent value that indicates whether the associated network is able to
receive traffic. The reachability status shall be able to distinguish, at a minimum, between "permanent" failures of a route,
such as might result from the failure of a router, and "temporary" unreachability due to the imposition of a congestion control
restriction.
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "r3_tree.h"
#include "bacnet_net.h"
#include "bacnet_object.h"

static inline uint16_t htons(uint16_t x)
{
    return __builtin_bswap16(x);
	//asm ("rev16 %0, %1" : "=r" (x) : "r" (x));
}

static const char* MSG_names[] = {
"Who_Is_Router_To_Network",
"I_Am_Router_To_Network",
"I_Could_Be_Router_To_Network",
"Reject_Message_To_Network",
"Router_Busy_To_Network",
"Router_Available_To_Network",
"Initialize_Routing_Table",
"Initialize_Routing_Table_Ack",
"Establish_Connection_To_Network",
"Disconnect_Connection_To_Network",
"Challenge_Request",
"Security_Payload",
"Security_Response",
"Request_Key_Update",
"Update_Key_Set",
"Update_Distribution_Key",
"Request_Master_Key",
"Set_Master_Key",
"What_Is_Network_Number",
"Network_Number_Is",
};
#define BROADCAST 0xFF
#define DATA_EXPECTING_REPLY 0x04

typedef struct _NextRouter NextRouter_t;
/*! 12.56.58 Routing_Table
This read-only property, of type BACnetLIST of BACnetRouterEntry, contains the table of first hop routers to remote
networks reachable through this port.
Router table entries shall contain the following information:
'network-number'    The network number reachable through the router specified by mac-address.
'mac-address'       The MAC address of the router on the network connected to this port that leads directly or
    indirectly to that network number.
'status'            Conveys whether the associated network is able to receive traffic. The values for this field are:
    AVAILABLE, BUSY, and DISCONNECTED.
'performance-index' This optional field is used to convey the performance index as conveyed in an I-Could-BeRouter-To-Network network layer message. See Clause 6.4.3.
*/
struct _NextRouter {
//    DataLink_t* datalink;
    BACnetAddr_t addr;
//    uint16_t network_number;
    enum {AVAILABLE, BUSY, DISCONNECTED} status:8;
    uint8_t  performance_index;
};
typedef struct _Network Network_t;
struct _Network {
    NextRouter_t next_router[2];
};

typedef struct _RoutingPort RoutingPort_t;
struct _RoutingPort {
    uint16_t network_number;
    DataLink_t* datalink;
    ttl_datalist_t routing_table;
};
struct _NetworkRoutingInfo {
    RoutingPort_t **network_ports;
};


extern int bacnet_network_security(DataLink_t* dl, BACnetNPDU*, DataUnit_t*);
static int bac_weak()
{
    return 0;
}
/* возможно мы не хотим использовать треды, тогда достаточно не включать в компиляющию все что относится к osThread */
#pragma weak bacnet_network_security = bac_weak


static int routing_add_network(ttl_datalist_t* routing_table, /* uint16_t network_number, */ uint16_t dnet, BACnetAddr_t* next_router);
static bool routing_get_next_router (ttl_datalist_t* routing_table, uint16_t dnet, void** destination);
static bool routing_network_is_reachable (ttl_datalist_t* routing_table, uint16_t dnet);
static int routing_network_available (ttl_datalist_t* routing_table, uint16_t dnet, BACnetAddr_t* next_router);
static int routing_network_busy      (ttl_datalist_t* routing_table, uint16_t dnet, BACnetAddr_t* next_router);
static int routing_network_busy_all  (ttl_datalist_t* routing_table, BACnetAddr_t*  next_router);
static int routing_network_enable_all(ttl_datalist_t* routing_table, BACnetAddr_t*  next_router);
static inline
Network_t * routing_get_network(ttl_datalist_t* routing_table, uint16_t dnet)
{
    return ttl_datalist_get(routing_table, dnet);
}

static int add_reachable(uint32_t key, void* value, void* user_data)
{
    Network_t* net = value;
    if (net->next_router[0].status == AVAILABLE || net->next_router[1].status ==AVAILABLE) {
        uint8_t *data = *(uint8_t**)user_data; // проще можно?
        *(uint16_t*)data = htons(key);
        *(uint8_t**)user_data = data;
    }
    return 0;
}
/*! \brief генерация запроса сетевого уровня */
int bacnet_network_message_req(DataLink_t * dl, BACnetNPDU *npci, DataUnit_t* tr)
{
    uint8_t* data = tr->buffer;
//    *data++ = npci->message_type;
    switch (npci->message_type) {
    case MSG_Network_Number_Is: {
        *data++ = dl->network_number>>8;
        *data++ = dl->network_number;
    } break;
    case MSG_Who_Is_Router_To_Network: {
        // все?
    } break;
    case MSG_I_Am_Router_To_Network: {
        DataLink_t** port= dl->device->network_ports;
        DataLink_t*p;
        if (port!=NULL)
        while ((p =*port++)!=NULL) {
            if (p->network_number != npci->SA.network_number) {
                *data++ = p->network_number>>8;
                *data++ = p->network_number;
                ttl_datalist_foreach(&p->routing_table, add_reachable, (void*)&data);
            }
        }
    } break;
    default:///
        break;
    }
    tr->size = data - tr->buffer;
    //if (tr->size)
    {
        npci->control = 0x80;
//        dl->destination_address = 0xFF;
//        tr->status = 0x0100;
//        npci->next_router = 0xFF;// туда
//        BACnetAddr_t destination_address;
//        bacnet_local_broadcast(dl, &destination_address);// настроить адрес
        tr->destination = NULL;// local_broadcast
        bacnet_datalink_request(dl, npci, tr);
    }
    return 0;
}

static ttl_datalist_t* routing_table_get(DataLink_t** network_ports, uint16_t network_number)
{
    DataLink_t** port = network_ports;
    DataLink_t*p;
    while ((p=*port++)!=NULL) {
        if (p->network_number==network_number) {
            return &p->routing_table;
        }
    }
    return NULL;
}
/*! \brief Обработка сетевых сообщений локальным устройством, возможно требуется знание Device_t

    network_ports - это свойство роутера, npci- свойство прикладного уровня или контекста APDU

    Сетевая безопасность завязана на этом уровне
    \return Need to be related
*/
int bacnet_network_message(DataLink_t* dl, BACnetNPDU *npci, DataUnit_t* tr)
{
    printf("Message type (%d) %s\n", npci->message_type,MSG_names[npci->message_type]);
    /// \see 6.2.4 Network Layer Message Type

    DataLink_t** network_ports = dl->device->network_ports;
    switch (npci->message_type){
    case MSG_Who_Is_Router_To_Network: {/*! If DNET is omitted, a router receiving this
message shall return a list of all reachable DNETs */
        uint32_t len = (tr->size)>>1;
        if (len) {
//            if (dl->routing_info==NULL) return 0;
            uint8_t* data = tr->buffer;
            uint16_t dnet_id = (data[0]<<8) | data[1]; //data+=2;
            // цикл по всем интерфейсам кроме данного
            DataLink_t** port= network_ports;
            DataLink_t*p;
            if (port!=NULL)
            while ((p =*port++)!=NULL) {

                if (routing_network_is_reachable (&p->routing_table, dnet_id)){
                    npci->message_type = MSG_I_Am_Router_To_Network;
                    tr->destination = NULL; // local broadcast
                    bacnet_datalink_request(dl, npci, tr);
                    return 0;//bacnet_network_message_req(dl/*, network_ports*/, npci, tr);
                }
                port++;
            }// while
        }
        else {// вернуть список всех доступных сетей
            npci->message_type = MSG_I_Am_Router_To_Network;
            return bacnet_network_message_req(dl/*, network_ports*/, npci, tr);
        }

    } break;
    case MSG_I_Am_Router_To_Network: {/*!		6.4.2 I-Am-Router-To-Network
This message is indicated by a Message Type of X'01' followed by one or more 2-octet network numbers. It is used to
indicate the network numbers of the networks accessible through the router generating the message. It shall always be
transmitted with a broadcast MAC address. */
//        if (dl->routing_info==NULL) return 0;
        uint32_t len = (tr->size)>>1;
        uint8_t* data = tr->buffer;
        ttl_datalist_t* routing_table = routing_table_get(network_ports, npci->SA.network_number);
        while (len--) {
            uint16_t dnet_id = (data[0]<<8) | data[1]; data+=2;
            routing_add_network(routing_table, dnet_id, &npci->SA);
        }
        // запомнить, назначть роутер по умолчанию
    } break;
    case MSG_Reject_Message_To_Network: {// сообщение не может быть доставлено из-за проблем с рутением
        uint8_t* data = tr->buffer;
        uint8_t reason = *data++;
        uint16_t dnet_id = (data[0]<<8) | data[1]; data+=2;
        /// 6.4.4 Reject-Message-To-Network
        /*
0: Other error.
1: The router is not directly connected to DNET and cannot find a router to DNET on any directly connected network using
   Who-Is-Router-To-Network messages.
2: The router is busy and unable to accept messages for the specified DNET at the present time.
3: It is an unknown network layer message type. The DNET returned in this case is a local matter.
4: The message is too long to be routed to this DNET.
5: The source message was rejected due to a BACnet security error and that error cannot be forwarded to the source device.
   See Clause 24.12.1.1 for more details on the generation of Reject-Message-To-Network messages indicating this reason.
6: The source message was rejected due to errors in the addressing. The length of the DADR or SADR was determined to be invalid
        */
    } break;
    case MSG_Router_Busy_To_Network: {/// 6.6.3.6 Router-Busy-To-Network
//        if (dl->routing_info==NULL) return 0;
        uint32_t len = (tr->size)>>1;
        uint8_t* data = tr->buffer;
        ttl_datalist_t* routing_table = routing_table_get(network_ports, npci->SA.network_number);
        if (len) {
            do {
                uint16_t dnet_id = (data[0]<<8) | data[1]; data+=2;
                routing_network_busy(routing_table, dnet_id, &npci->SA);
            } while (--len);
        } else {
            routing_network_busy_all(routing_table, &npci->SA);
        }
    } break;
    case MSG_Router_Available_To_Network: {
//        if (dl->routing_info==NULL) return 0;
        uint32_t len = (tr->size)>>1;
        uint8_t* data = tr->buffer;
        ttl_datalist_t* routing_table = routing_table_get(network_ports, npci->SA.network_number);
        if (len){
            do {
                uint16_t dnet_id = (data[0]<<8) | data[1]; data+=2;
                routing_network_available(routing_table, dnet_id, &npci->SA);
            } while (--len);
        } else {
//            ttl_datalist_foreach(routing_table, try_enable, (void*)(uintptr_t)next_router);

            routing_network_enable_all(routing_table, &npci->SA);
        }
    } break;
    case MSG_What_Is_Network_Number: {
        uint8_t* data = tr->buffer = malloc (32)+16;
        if (dl->network_number!=GLOBAL_BROADCAST) {
            npci->message_type = MSG_Network_Number_Is;
            data[0] = dl->network_number>>8;
            data[1] = dl->network_number;
            tr->size = 2;
            tr->destination = NULL;// local broadcast
            npci->control = 0x80;
            bacnet_datalink_request(dl, npci, tr);// адрес назначения берется из npci source
        }
    } break;
    case MSG_Network_Number_Is: {
        uint8_t* data = tr->buffer;
        uint16_t net_id = (data[0]<<8) | data[1]; data+=2;
        if (dl->network_number==LOCAL_NETWORK) {// запомнить номер
            dl->network_number = net_id;
            printf("-- set network number =%d\n", net_id);
        } else
        if (dl->network_number!= net_id) {
            // сообщить о конфликте, через что?
        }
    } break;
    case MSG_Challenge_Request: //0x0A,
	case MSG_Security_Payload:  //0x0B,
	case MSG_Security_Response: //0x0C,
	case MSG_Request_Key_Update://0x0D,
	case MSG_Update_Key_Set:    //0x0E,
	case MSG_Update_Distribution_Key://0x0F,
	case MSG_Request_Master_Key://0x10,
	case MSG_Set_Master_Key:    //0x11,
        return bacnet_network_security(dl, npci, tr);// возвращает transfer если расшифровал? эти запросы после расшифровки могут возвращаться обратно
    default:

        break;
    }
    return 0;
}
#if 0
static int vmac_translate(uint32_t key, void *data, void *user_data)
{
    BACnetAddr_t* addr = user_data;
    if (__builtin_memcmp(data, addr->ipv6_addr, 16)==0) {
        addr->vmac_addr = key;
        addr->len = 3;
        return 1;
    }
    return 0;
}
#endif // 0
/*! \brief Доставка сообщений
    dataunit_ref();
 */
int bacnet_relay(DataLink_t** network_ports, BACnetNPDU *npci, DataUnit_t* transfer)
{

    /// переместить в datalink
#if 0
    if (dl->mac_len==16 && npci->SA.network_number == dl->network_number) {// network source address translation
        // в случае когда используется трансляция адресов
        if (ttl_datalist_foreach(&dl->vmac_table, /* dl-> */vmac_translate, &npci->SA)==NULL){
            return 0; // отбросили пакет
        }
    }
#endif // 0
    /// Bit 5: Destination specifier установлен бит 5 адресовано в другую сеть, все сообщения для другой сети
    if (--npci->hop_count == 0) return 0;
    DataLink_t** port = network_ports;// по списку соседних портов, кроме своего
    if (npci->DA.network_number==GLOBAL_BROADCAST) {
        DataLink_t* p;
        while((p=*port++)!=NULL) {
            if (npci->SA.network_number != p->network_number){// except network of origin
                //bacnet_local_broadcast(datalink, &destination_address);
                transfer->destination = NULL;
                bacnet_datalink_request(p, npci, transfer);
            }
        }
    }
    else {

        DataLink_t* p;

        while((p=*port++)!=NULL) {
            if (npci->DA.network_number == p->network_number) {/// Is message destined for a directly connected network? -- Yes
                /// Remove DNET and DADR from NPCI and send
                npci->control &= ~0x20; // -- само удалится?
                transfer->destination = (&npci->DA);// создать объект или найти похожую запись в талице локальных хостов.
                bacnet_datalink_request(p, npci, transfer);
                return 0;
            }
        }
        /// Is message destined for a directly connected network? -- No.
        port = network_ports;
        while((p=*port++)!=NULL) {
            if (npci->SA.network_number == p->network_number) {
            } else
            if (routing_get_next_router(&p->routing_table, npci->DA.network_number, &transfer->destination))
            {
//                npci.DA <= next_router->mac_address;
                /// Is identity of router for DNET on directly connected net known? Send message to next router
                bacnet_datalink_request(p, npci, transfer);// запросить передачу данных
                return 0;
            }
        }
        /// \todo Attempt to locate router for DNET? Who_Is_Router to network?
        /// ---- эту часть вынуть отсюда
        /// Иначе вернуть сообщение MSG_Reject_Message_To_Network
        npci->message_type = MSG_Reject_Message_To_Network;
        /// заполнить поле return reason;
        //return bacnet_network_message_req(dl, npci, transfer);
    }
    return 0;
}

/*! 6.6.2 Start-up Procedures
Upon start-up, each router shall broadcast out each port an I-Am-Router-To-Network message containing the network
numbers of each accessible network except the networks reachable via the network on which the broadcast is being made.
This enables routers to build or update their routing table entries for each of the network numbers contained in the message.*/
static int routing_network_available(ttl_datalist_t* routing_table, uint16_t dnet, BACnetAddr_t* router)
{
    Network_t* net = ttl_datalist_get(routing_table, dnet);
    if (net==NULL) {
    } else
    if (net->next_router[0].addr.vmac_addr==router->vmac_addr){
        net->next_router[0].status = AVAILABLE;
        return 1;
    } else
    if (net->next_router[1].addr.vmac_addr==router->vmac_addr){
        net->next_router[1].status = AVAILABLE;
        return 1;
    }
    return 0;
}
static int routing_network_busy(ttl_datalist_t* routing_table, uint16_t dnet, BACnetAddr_t* router)
{
    Network_t* net = ttl_datalist_get(routing_table, dnet);
    if (net== NULL) {
    } else
    if (net->next_router[0].addr.vmac_addr==router->vmac_addr){
        net->next_router[0].status = BUSY;
        return 1;
    } else
    if (net->next_router[1].addr.vmac_addr==router->vmac_addr){
        net->next_router[1].status = BUSY;
        return 1;
    }
    return 0;
}
static int try_busy (uint32_t key, void* value, void* user_data)
{
    Network_t* net = value;
//    uint16_t network_number = key;
    uint32_t  vmac = (uintptr_t)user_data;
    if (net->next_router[0].addr.vmac_addr == vmac){
        net->next_router[0].status = BUSY;
    } else
    if (net->next_router[1].addr.vmac_addr == vmac){
        net->next_router[1].status = BUSY;
    }
    return 0;
}
static int try_enable (uint32_t key, void* value, void* user_data)
{
    Network_t* net = value;
//    uint16_t network_number = key;
    uint32_t  vmac = (uintptr_t)user_data;
    if (net->next_router[0].addr.vmac_addr == vmac){
        net->next_router[0].status = AVAILABLE;
    } else
    if (net->next_router[1].addr.vmac_addr == vmac){
        net->next_router[1].status = AVAILABLE;
    }
    return 0;
}
static int routing_network_busy_all(ttl_datalist_t* routing_table, BACnetAddr_t*  next_router)
{
    ttl_datalist_foreach(routing_table, try_busy, (void*)(uintptr_t)next_router);
    return 0;
}
static int routing_network_enable_all(ttl_datalist_t* routing_table, BACnetAddr_t*  next_router)
{
    ttl_datalist_foreach(routing_table, try_enable, (void*)(uintptr_t)next_router);
    return 0;
}
static int routing_add_network(ttl_datalist_t* routing_table, uint16_t dnet, BACnetAddr_t* next_router)
{
    Network_t* net = ttl_datalist_get(routing_table, dnet);
    if (net==NULL) {
        net = g_slice_alloc(sizeof(Network_t));// не освобождаем
        //net->next_router[0].network_number = network_number;

        net->next_router[0].addr.vmac_addr = next_router->vmac_addr;
        net->next_router[0].status = AVAILABLE;// слабая
        net->next_router[0].performance_index = ~0;

        //net->next_router[1].network_number = 0xFFFF;
        net->next_router[1].addr.vmac_addr = ~0;
        net->next_router[1].status = ~0;// никакая
        net->next_router[1].performance_index = ~0;
        ttl_datalist_push(routing_table, dnet, net);
        return 0;
    }
    if(/* (net->next_router[0].network_number == network_number) && */(net->next_router[0].addr.vmac_addr == next_router->vmac_addr)) {
        if(net->next_router[0].performance_index) net->next_router[0].performance_index--;
    } else
    if(/* (net->next_router[1].network_number == network_number) && */(net->next_router[1].addr.vmac_addr == next_router->vmac_addr)) {
        if(net->next_router[0].performance_index) net->next_router[0].performance_index--;
    } else {
        if (net->next_router[0].performance_index <= net->next_router[1].performance_index)
        {// более доступная сеть имеет меньший показатель
            //net->next_router[1].network_number = network_number;
            net->next_router[1].addr.vmac_addr = next_router->vmac_addr;
            net->next_router[1].status = AVAILABLE;// слабая
            net->next_router[1].performance_index = ~0;
        } else {
            //net->next_router[0].network_number = network_number;
            net->next_router[0].addr.vmac_addr = next_router->vmac_addr;
            net->next_router[0].status = AVAILABLE;// слабая
            net->next_router[0].performance_index = ~0;
        }
    }
    return 0;
}
static bool routing_get_next_router (ttl_datalist_t* routing_table, uint16_t dnet, void** router)
{
    Network_t* net = ttl_datalist_get(routing_table, dnet);
    if (net==NULL) return false;
    NextRouter_t* next_router = (net->next_router[0].performance_index <= net->next_router[1].performance_index)?
                                &net->next_router[0]: &net->next_router[1];
//    router->network_number = 0; // не определена
    *router = next_router;
    /// копировать адрес
    return true;
}
/*! \brief проверяет доступность сети
    \param dnet - куда доставить, номер сети
    \param snet - источник откуда пришле запрос, если роутер расположен в той же сети, то не доставляем, ответ отрицательный
*/
static bool routing_network_is_reachable (ttl_datalist_t* routing_table, uint16_t dnet)
{
    Network_t* net = ttl_datalist_get(routing_table, dnet);
    if (net==NULL) return false;
    if (/*(net->next_router[0].network_number!= snet) && */(net->next_router[0].status = AVAILABLE)) return true;
    if (/*(net->next_router[1].network_number!= snet) && */(net->next_router[1].status = AVAILABLE)) return true;
    return false;
}

void bacnet_network_test(DataLink_t* datalink)
{
//    DataLink_t* datalink = device->network_ports[0];
    datalink->network_number = 1;
    DataUnit_t transfer;
    BACnetNPDU npci;
    DataUnit_t *tr = &transfer;//g_slice_alloc(sizeof(DataUnit_t));
    tr->buffer = malloc(32)+16;
    tr->size   = 0;
    tr->status = 0;
    tr->destination = NULL;// local broadcast
    npci.control = 0x80;
    npci.message_type = MSG_Network_Number_Is;//MSG_Who_Is_Router_To_Network;
    bacnet_network_message_req(datalink, &npci, tr);

    tr->buffer = malloc(32)+16;
    tr->size   = 0;
    tr->status = 0;
    npci.message_type = MSG_What_Is_Network_Number;//MSG_I_Am_Router_To_Network;//MSG_Who_Is_Router_To_Network;
    bacnet_network_message_req(datalink, &npci, tr);
}
