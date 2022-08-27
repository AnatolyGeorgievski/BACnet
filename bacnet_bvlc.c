#include "bacnet_net.h"

/*! \todo реализовать блек листы и access_list!
    \todo NAT_traversal Global_IP
*/

#include <time.h>
#include <stdint.h>
#include <stdio.h>

static inline uint16_t ntohs(uint16_t x)
{
    return __builtin_bswap16(x);
	//asm ("rev16 %0, %1" : "=r" (x) : "r" (x));
}
static inline uint16_t htons(uint16_t x)
{
    return __builtin_bswap16(x);
	//asm ("rev16 %0, %1" : "=r" (x) : "r" (x));
}

/*! \defgroup _annex_j ANNEX J - BACnet/IP (NORMATIVE)

    \see ANSI/ASHRAE Addendum aj to ANSI/ASHRAE Standard 135-2012
    \see ANSI/ASHRAE Std. 135-2016 ANNEX U - BACnet/IPv6 (NORMATIVE)
    \see ISO 16484-5:2017 Building automation and control systems (BACS) - Part 5: Data communication protocol
*/

/*!
B/IP devices shall support configurable IP addresses and each shall be able to be set to any valid unicast IP address. B/IP devices
shall also support a configurable UDP port number and shall support, at a minimum, values in the ranges 47808 (0xBAC0) - 47823 (0xBACF) and
49152 (0xC000) - 65535 (0xFFFF). For B/IP devices that support multiple B/IP ports, the UDP port number for each B/IP port shall be settable
across the above noted valid range */
/*! NetworkPortObject должен поддерживать следуюшие свойства
BACnet_IP_Mode
BACnet_IP_UDP_Port
BACnet_IP_Multicast_Address
-- для работы устройства за NAT
BACnet_IP_NAT_Traversal
BACnet_IP_Global_Address

BACnet_IPv6_Mode
BACnet_IPv6_UDP_Port
BACnet_IPv6_Multicast_Address
-- для режима BBMD BACnet/IP broadcast management device
BBMD_Broadcast_Distribution_Table
BBMD_Accept_FD_Registrations
BBMD_Foreign_Device_Table
-- для режима FOREIGN
FD_BBMD_Address
FD_Subscription_Lifetime
*/

/*! \brief
    \param address in network format
*/
/* Broadcast Address - stored in network byte order */
//static struct sockaddr BIP_Broadcast_Address;

static int bac_weak()
{
    return 0;
}
/* возможно мы не хотим использовать треды, тогда достаточно не включать в компиляющию все что относится к osThread */
#pragma weak bacnet_ip_sendto = bac_weak

typedef struct _FDT BlackList_t;
typedef struct _FDT FDT_t;
typedef struct _BDT BDT_t;
struct _BDT {
    struct {
        union {
            uint32_t        in_addr;
        };
        uint16_t in_port;
    };
    struct _BDT* next;
};
struct _FDT {
    struct {
        union {
            uint32_t        in_addr;
        };
        uint16_t in_port;
    };
    uint16_t ttl;
    time_t timestamp;// в секундах
    struct _FDT* next;
};

/*! \brief регистрация нового внешнего устройства */
static int FDT_register(DataLink_t *dl, BACnetAddr_t *addr, uint16_t ttl)
{
    // int clock_gettime(clockid_t clock_id, struct timespec *tp);
    time_t timestamp = time(NULL);//
    FDT_t* entry = ((DataLink_IPv4_t*)dl)->FDT;
    while (entry){
        if ((timestamp - entry->timestamp) > entry->ttl) {
            break;
        }
        entry = entry->next;
    }
    if (entry == NULL) {// создать новую запись
        entry = g_slice_alloc(sizeof(FDT_t));
        entry->next= ((DataLink_IPv4_t*)dl)->FDT;// foreign device table
        ((DataLink_IPv4_t*)dl)->FDT = entry;
    }
    entry->in_addr = addr->in_addr;
    entry->in_port = addr->in_port;
    entry->ttl = ttl+30;// seconds+grace period
    entry->timestamp = timestamp;//bacnet_timesec();
    // ввыполнить операцию замены атомарно
    return 0;
}
/*! \brief удалить запись из таблицы внешних устройств */
static uint16_t FDT_unregister(DataLink_t *dl, uint8_t *buffer)
{
    uint32_t in_addr = *(uint32_t*)&buffer[0];// для NAT traversal GlobalAddress
    uint16_t in_port = *(uint16_t*)&buffer[4];
    FDT_t* entry = ((DataLink_IPv4_t*)dl)->FDT;
    while(entry){
        if (entry->in_addr == in_addr && entry->in_port == in_port) {
            entry->ttl = 0;
            return 0x0000;//ACK
        }
        entry=entry->next;
    }
    return 0x0050;//NACK
}
/*! \brief заполняет шапку пакета для рассылик методом Forwarded по получателям FDT и BDT */
static void BBMD_forwardHdr(DataLink_t* dl, BACnetAddr_t* source_addr, uint8_t *buffer, size_t length)
{
    buffer[0] = BVLL_TYPE_BACNET_IP;
    buffer[1] = BVLC_Forwarded_NPDU;
    *(uint16_t*)&buffer[2] = htons(length);
    *(uint32_t*)&buffer[4] = source_addr->in_addr;
    *(uint16_t*)&buffer[8] = source_addr->in_port;
}
/*! \brief рассылает сообщения типа Forwarded по получателям в сети FDT и BDT */
static int BBMD_distribute(DataLink_t* dl, BACnetAddr_t* source_addr, uint8_t *buffer, size_t length)
{
    uint32_t in_addr = source_addr->in_addr;
    uint16_t in_port = source_addr->in_port;
    BACnetAddr_t dest_addr;
    dest_addr.len=6;
    dest_addr.network_number=0;
    time_t timestamp = time(NULL);
    FDT_t* entry = ((DataLink_IPv4_t*)dl)->FDT;
    while(entry) {
        // если время хранения записи не истекло и запись не пустая
        if ((timestamp - entry->timestamp) < entry->ttl) {
            if (entry->in_addr != in_addr || entry->in_port != in_port) {
                dest_addr.in_addr = entry->in_addr;
                dest_addr.in_port = entry->in_port;
                //res =
                (void)bacnet_ip_sendto(dl, &dest_addr, buffer, length);
            }
        } else {
            /// \todo удалить запись из FDT
        }
        entry=entry->next;
    }
    {// разослать по всем рутерам, обратно не слать
        BDT_t* entry = ((DataLink_IPv4_t*)dl)->BDT;
        while(entry) {
            if (entry->in_addr != in_addr || entry->in_port != in_port) {
                dest_addr.in_addr = entry->in_addr;
                dest_addr.in_port = entry->in_port;
                (void)bacnet_ip_sendto(dl, &dest_addr, buffer, length);
            }
            entry=entry->next;
        }
    }
    return 0;
}
static int BBMD_result_code(DataLink_t* dl, BACnetAddr_t* dest_addr, uint8_t *buffer, uint16_t result)
{
    buffer[0] = BVLL_TYPE_BACNET_IP;
    buffer[1] = BVLC_Result;
    *(uint16_t*)&buffer[2] = htons(6);
    *(uint16_t*)&buffer[4] = htons(result);
    return bacnet_ip_sendto(dl, dest_addr, buffer, 6);
}
/*! \brief проверяет содержится ли адрес в таблице распространения широковещательных пакетов */
static bool BBMD_BDT_member (DataLink_t* dl, BACnetAddr_t* source_addr)
{
    uint32_t in_addr = source_addr->in_addr;
    uint16_t in_port = source_addr->in_port;
    BDT_t* entry = ((DataLink_IPv4_t*)dl)->BDT;
    while(entry) {
        if (entry->in_addr == in_addr && entry->in_port == in_port) {
            return true;
        }
        entry=entry->next;
    }
    return false;
}
/*! \brief рассылает сообщение в локальной сети */
static int BVLC_broadcast  (DataLink_t* dl, uint8_t *buffer, size_t length)
{
    BACnetAddr_t dest_addr;
    dest_addr.len=6;
    dest_addr.in_addr = ((DataLink_IPv4_t*)dl)->FD_BBMD_address.in_addr;
    dest_addr.in_port = ((DataLink_IPv4_t*)dl)->FD_BBMD_address.in_port;//htons(PORT_BACNET_IP);
    return bacnet_ip_sendto(dl, &dest_addr, buffer, length);
}
static void black_list_push(FDT_t **top, BACnetAddr_t* addr, uint16_t ttl)
{
    FDT_t *entry = g_slice_alloc(sizeof(FDT_t));
    entry->in_addr = addr->in_addr;
    entry->in_port = addr->in_port;
    entry->ttl = ttl+30;// seconds+grace period
    entry->next= *top;
    *top = entry;
}
static bool black_list_test(DataLink_t *dl, BACnetAddr_t* addr)
{
    time_t timestamp = time(NULL);/// \todo переделать на clock_gettime(MONOTONIC)
    FDT_t* entry = ((DataLink_IPv4_t*)dl)->blacklist;
    FDT_t* prev  = NULL;
    while(entry){
        if ((timestamp - entry->timestamp) < entry->ttl) {// исключить из списка
            if(prev ==NULL) {
                ((DataLink_IPv4_t*)dl)->blacklist=NULL;//entry->next;
            } else {
                prev->next = NULL;//entry->next;
            }
            while (entry) {
                FDT_t* garbage = entry;// весь список дальше можно убить? потому что добавление и обновление перекладывает список с начала
                entry = entry->next;
                g_slice_free1(sizeof(FDT_t), garbage);
            }
            return 0;
        } else
        if (entry->in_addr == addr->in_addr) {
            entry->timestamp = timestamp;// обновили штамп времени
            if(prev !=NULL) {// переместили в начало списка
                prev->next = entry->next;
                entry->next = ((DataLink_IPv4_t*)dl)->blacklist;// можно атомарно заменить вершину
                ((DataLink_IPv4_t*)dl)->blacklist = entry;// в начало списка
            }
            return 1;
        } else {
            prev =entry;
            entry=entry->next;
        }
    }
    return 0;
}
/*! \brief Разбор пакета, для сети BACnet/IP

    J.2 BACnet Virtual Link Layer

*/
int bacnet_bvlc_indication (DataLink_t* dl, BACnetAddr_t *source_addr, uint8_t * buffer, size_t size)
{
    uint8_t BVLC_type = buffer[0]; // 0x81 - BACnet/IP; 0x82 - BACnet/IPv6
    if (BVLC_type!=BVLL_TYPE_BACNET_IP) {
        printf("BVLC type mismatch\n");
        return -1;
    }
    /// \todo трансляция адресов ради NAT_Traversal!!
    source_addr->network_number = dl->network_number;
    uint8_t BVLC_function = buffer[1];
    uint16_t BVLC_length  = ntohs(*(uint16_t*)&buffer[2]);
    if (BVLC_length!= size) {
        printf("BVLC length mismatch\n");
        return -2;/// \todo навести порядок в кодах ошибок
    }
    if (0 && black_list_test(dl, source_addr)) {
        printf("BVLC blacklist\n");
        return -13;
    }
//    printf("BVLC Type: %02X, Function: %02X, Length: %d\n", BVLC_type, BVLC_function, BVLC_length);
    int res = 0;
//    DataUnit_t transfer;
//    DataUnit_t *tr=&transfer;//g_slice_alloc(sizeof(DataUnit_t));//={.buffer = buffer+4,.size   = BVLC_length-4};
    switch(BVLC_function) {
    case BVLC_Result:{// J.2.1 BVLC-Result
/*! Result Code: 2-octets
            X'0000' Successful completion
            X'0010' Write-Broadcast-Distribution-Table NAK
            X'0020' Read-Broadcast-Distribution-Table NAK
            X'0030' Register-Foreign-Device NAK
            X'0040' Read-Foreign-Device-Table NAK
            X'0050' Delete-Foreign-Device-Table-Entry NAK
            X'0060' Distribute-Broadcast-To-Network NAK
*/
        uint16_t resultCode = ntohs(*(uint16_t*)&buffer[4]);
        printf("\tBVLC-Result: %04X\n", resultCode);
    } break;
    case BVLC_Write_Broadcast_Distribution_Table: // J.2.2 Write-Broadcast-Distribution-Table
    case BVLC_Read_Broadcast_Distribution_Table:
    case BVLC_Read_Broadcast_Distribution_Table_Ack: {//
/*! This message provides a mechanism for initializing or updating a Broadcast Distribution Table (BDT) in a BACnet Broadcast
Management Device (BBMD) */
        /// That function is now performed by writes to the Network Port object.
    } break;
    case BVLC_Forwarded_NPDU: {// J.2.5 Forwarded-NPDU
/*! Upon receipt of a Forwarded-NPDU with a B/IP Address of Originating Device field whose B/IP address is different from the
B/IP address of the sending node, the receiving node shall utilize the contents of that field as the source B/IP address of the
sending node.*/
        // заполнить source_addr из шапки
        source_addr->in_addr = *(uint32_t*)&buffer[4];
        source_addr->in_port = *(uint16_t*)&buffer[8];
        source_addr->len = 6;
        if (((DataLink_IPv4_t*)dl)->mode==IP_Mode_BBMD){// отослать broadcast
//            BBMD_forwardHdr(dl, source_addr, buffer-6, BVLC_length+6);
            if (BBMD_BDT_member(dl, source_addr))
            {/*  Broadcast locally if received via unicast from a BDT member */
                BVLC_broadcast (dl, buffer-6, BVLC_length+6);
            }
        } else {
            res = bacnet_datalink_indication(dl, source_addr, buffer+10, BVLC_length-10);
        }
    } break;
    case BVLC_Register_Foreign_Device: {// J.2.6 Register-Foreign-Device
/*! This message allows a foreign device, as defined in Clause J.5.1, to register with a BBMD for the purpose of receiving broadcast
messages. */
        uint16_t resultCode = 0;
        uint16_t ttl = ntohs(*(uint32_t*)&buffer[4]);
        if (((DataLink_IPv4_t*)dl)->mode==IP_Mode_BBMD && ((DataLink_IPv4_t*)dl)->BBMD_Accept_FD_Registrations) {
            res = FDT_register(dl, source_addr, ttl);
        } else { // отослать NACK
            resultCode = 0x0030;// NACK
        }
        res = BBMD_result_code(dl, source_addr, buffer, resultCode);
    } break;
    case BVLC_Read_Foreign_Device_Table: {
        uint16_t resultCode = 0;
        if (((DataLink_IPv4_t*)dl)->mode==IP_Mode_BBMD && ((DataLink_IPv4_t*)dl)->BBMD_Accept_FD_Registrations) {
            resultCode = 0x0040;// заполнить ответ
        } else { // отослать NACK
            resultCode = 0x0040;// NACK
        }
        res = BBMD_result_code(dl, source_addr, buffer, resultCode);
    } break;
    case BVLC_Read_Foreign_Device_Table_Ack: {// я просил?
    } break;
    case BVLC_Delete_Foreign_Device_Table_Entry: {// J.2.9 Delete-Foreign-Device-Table-Entry
        uint16_t resultCode = 0;
        if (((DataLink_IPv4_t*)dl)->mode==IP_Mode_BBMD && ((DataLink_IPv4_t*)dl)->BBMD_Accept_FD_Registrations) {
            resultCode = FDT_unregister(dl, &buffer[4]);
        } else {
            resultCode = 0x0050;// NACK
        }
        res = BBMD_result_code(dl, source_addr, buffer, resultCode);
    } break;
    case BVLC_Distribute_Broadcast_To_Network: {// J.2.10 Distribute-Broadcast-To-Network
/*! Upon receipt of a BVLL Distribute-Broadcast-To-Network message from a registered foreign device, the receiving BBMD shall
transmit a BVLL Forwarded-NPDU message on its local IP subnet using the local B/IP broadcast address as the destination
address. */
        uint16_t resultCode = 0;
        if (((DataLink_IPv4_t*)dl)->mode==IP_Mode_BBMD)
        {// если устройство работает как BBMD, отослать каждому в BDT и в FDT
            BBMD_forwardHdr(dl, source_addr, buffer-6, BVLC_length+6);
            BVLC_broadcast (dl,              buffer-6, BVLC_length+6);
            BBMD_distribute(dl, source_addr, buffer-6, BVLC_length+6);
        } else {// NACK
            resultCode = 0x0060;// NACK
        }
        res = BBMD_result_code(dl, source_addr, buffer-6, resultCode);
        res = bacnet_datalink_indication(dl, source_addr, buffer+4, BVLC_length-4);
    } break;
    case BVLC_Original_Unicast_NPDU:// J.2.11 Original-Unicast-NPDU
        printf("\tOriginal-Unicast-NPDU:\n");
        res = bacnet_datalink_indication(dl, source_addr, buffer+4, BVLC_length-4);
        break;
    case BVLC_Original_Broadcast_NPDU:// J.2.12 Original-Broadcast-NPDU
        printf("\tOriginal-Broadcast-NPDU:\n");
        if (((DataLink_IPv4_t*)dl)->mode==IP_Mode_BBMD)
        {// отослать всем устройставам из FDT списка
            BBMD_forwardHdr(dl, source_addr, buffer-6, BVLC_length+6);// добавить заголовок Forwarded-NPDU
            BBMD_distribute(dl, source_addr, buffer-6, BVLC_length+6);// не меняет содержимое пакета
        }
        printf ("size = %d\n", BVLC_length);
        int i;
        for(i=0;i<BVLC_length-4;i++) {
            printf (" %02X", buffer[i+4]);
        }
        printf ("\n");
        res = bacnet_datalink_indication(dl, source_addr, buffer+4, BVLC_length-4);// содержимое пакета может меняться
        break;
    default: break;
    }
    return res;
}


