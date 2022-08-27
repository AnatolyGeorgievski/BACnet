/*! IPv6 BVLL -- Virtual Logical Level

Сергей

    U.5 BACnet /IPv6 VMAC Table Management

    The Virtual MAC address table shall be updated using the respective parameter values of the incoming messages. For
outgoing messages to a VMAC address that is not in the table, the device shall transmit an Address-Resolution message. The
Virtual MAC Address table shall be updated with the values conveyed in the Address-Resolution-ACK message.
    To learn the VMAC address of a remote BACnet device with a known B/IPv6 address, a B/IPv6 node may send a
VirtualAddress-Resolution message to that device and use the information of the Virtual-Address-Resolution-ACK message to
update the VMAC table.
    Upon receipt of a Virtual-Address-Resolution message, the receiving node shall construct a Virtual-Address-Resolution-ACK
message whose Source-Virtual-Address contains its virtual address and transmit it via unicast to the B/IPv6 node that
originally initiated the Virtual-Address-Resolution message.
    Upon receipt of an Address-Resolution or Forwarded-Address-Resolution message whose target virtual address is itself, a
B/IPv6 node shall construct an Address-Resolution-ACK message and send it via unicast to the B/IPv6 node that originally
initiated the Address-Resolution message.
    In addition to forwarding NPDUs to other BBMDs and foreign devices, a B/IPv6 BBMD is used in determining the VMAC
address of a B/IPv6 node that is not reachable by multicasts or is registered as a foreign device. See Clause U.4.4.
*/

#include "bacnet_net.h"
#include <time.h>
#include <stdint.h>
#include <stdio.h>

typedef struct _FDT  FDT_t;
typedef struct _BDT  BDT_t;
typedef struct _VMAC VMAC_t;

/*!
 The Broadcast Distribution Table shall not contain an entry for the BBMD in which the BDT resides */

struct _BDT {
    IPv6_Addr_t in6_addr;
    uint16_t in_port;
    uint16_t _unused1;
    char* fqdn_name;// DNS name
    BDT_t * next;
};
struct _FDT {
    struct {
        IPv6_Addr_t in6_addr;
        uint16_t in_port;
    };
    uint16_t ttl;
    uint32_t virtual_address;// 22 бита
    time_t timestamp;
    FDT_t * next;
};
struct _VMAC {
    struct {
        IPv6_Addr_t in6_addr;
        uint16_t in_port;
    };
    time_t timestamp;
//    VMAC_t * next;
};

/*
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

// преобразование для Little-Endian архитектуры
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

static inline uint32_t vmac_address_load(uint8_t* buffer)
{
    uint32_t vmac = (buffer[0]<<16) | ntohs(*(uint16_t*)&buffer[1]);//(buffer[1]<<8) | (buffer[2]);
    return vmac;
}
static inline void vmac_address_store(uint32_t vaddr, uint8_t* buffer)
{
    buffer[0] = vaddr>>16;
    *(uint16_t*)&buffer[1] = htons(vaddr);
}
static int bac_weak()
{
    return 0;
}
#pragma weak bacnet_ipv6_sendto = bac_weak
/*! \brief добавить запись в таблицу виртуальных адресов */
static void vmac_address_add(DataLink_IPv6_t *dl, uint32_t virtual_addr, uint8_t* mac)
{
    VMAC_t* vmac = g_slice_alloc(sizeof(VMAC_t));
    vmac->in6_addr = *(IPv6_Addr_t*)&mac[0];
    vmac->in_port = *(uint16_t*)&mac[16];
    vmac = ttl_datalist_set(&dl->vmac_table, virtual_addr, vmac);
    if (vmac != NULL) {
        g_slice_free1(sizeof(VMAC_t), vmac);
    }
}
/*! \brief добавить запись в таблицу виртуальных адресов и транслировать адрес */
static void vmac_address_set(DataLink_IPv6_t* dl, BACnetAddr_t * addr, uint32_t virtual_addr)
{
    VMAC_t* vmac = g_slice_alloc(sizeof(VMAC_t));
    vmac->in6_addr = *(IPv6_Addr_t*)addr->ipv6_addr;
    vmac->in_port = *(uint16_t*)addr->in_addr;
    vmac = ttl_datalist_set(&dl->vmac_table, virtual_addr, vmac);
    if (vmac != NULL) {
        g_slice_free1(sizeof(VMAC_t), vmac);
    }
    addr->vmac_addr = virtual_addr;
    addr->len = 3;
}
/*! \brief рассылает сообщения типа Forwarded по получателям в сети FDT и BDT */
static  int BBMD_distribute(DataLink_t* dl, uint32_t source_virtual_address, uint8_t *buffer, size_t length)
{
    BACnetAddr_t dest_addr;
    dest_addr.len=6;
    dest_addr.network_number=0;
    time_t timestamp = time(NULL);
    FDT_t* entry = ((DataLink_IPv6_t*)dl)->FDT;
    while(entry) {
        // если время хранения записи не истекло и запись не пустая
        if ((timestamp - entry->timestamp) < entry->ttl) {
            if (entry->virtual_address != source_virtual_address) {
                dest_addr.ipv6_addr = (uint8_t*)&entry->in6_addr;
                dest_addr.in_port = entry->in_port;
                bacnet_ipv6_sendto(dl, &dest_addr, buffer, length);
            }
        } else {
            /// \todo удалить запись из FDT
        }
        entry=entry->next;
    }
    {// разослать по всем рутерам
        BDT_t* entry = ((DataLink_IPv6_t*)dl)->BDT;
        while(entry) {
//            vmac_address_store(entry->vmac_addr, &buffer[]);// подправить поле
            dest_addr.ipv6_addr = (uint8_t*)&entry->in6_addr;
            dest_addr.in_port = entry->in_port;
            bacnet_ipv6_sendto(dl, &dest_addr, buffer, length);
            entry=entry->next;
        }
    }
    return 0;
}
/*! \brief заполняет шапку пакета для рассылик методом Forwarded по получателям FDT и BDT */
static void BBMD_forwardHdr (BACnetAddr_t* source_addr, uint32_t source_virtual_addr, uint8_t *buffer, size_t length)
{
    buffer[0] = BVLL_TYPE_BACNET_IPv6;
    buffer[1] = BVLL_Forwarded_NPDU;
    *(uint16_t*)&buffer[2] = htons(length);
    vmac_address_store(source_virtual_addr, &buffer[4]);// Original-Source-Virtual-Address = source_addr->vmac_addr;
    *(IPv6_Addr_t*)&buffer[7] = *(IPv6_Addr_t*)source_addr->ipv6_addr;// Original-Source-B/IPv6-Address
    *(uint16_t*)&buffer[7+16] = source_addr->in_port;
}
/*! \brief U.2.5 Forwarded-Address-Resolution
This message is unicast by B/IPv6 BBMDs to determine the B/IPv6 address of a known virtual address belonging to a
different multicast domain.
The Forwarded-Address-Resolution message is unicast to each address in the broadcast distribution and foreign device tables.
*/
static void BBMD_forwardAR  (BACnetAddr_t* source_addr, uint32_t source_virtual_addr, uint32_t target_virtual_addr, uint8_t *buffer)
{
    buffer[0] = BVLL_TYPE_BACNET_IPv6;
    buffer[1] = BVLL_Forwarded_Address_Resolution;
    *(uint16_t*)&buffer[2] = htons(28);
    vmac_address_store(source_virtual_addr, &buffer[4]);// Original-Source-Virtual-Address = source_addr->vmac_addr;
    vmac_address_store(target_virtual_addr, &buffer[7]);// Original-Source-Virtual-Address = source_addr->vmac_addr;
    *(IPv6_Addr_t*)&buffer[10] = *(IPv6_Addr_t*)source_addr->ipv6_addr;// Original-Source-B/IPv6-Address
    *(uint16_t*)&buffer[10+16] = source_addr->in_port;
}
static  int BBMD_broadcast  (DataLink_t* dl, uint8_t *buffer, size_t length)
{
    BACnetAddr_t dest_addr;
    dest_addr.len=18;
    dest_addr.ipv6_addr = (uint8_t*)&((DataLink_IPv6_t*)dl)->FD_BBMD_Address.in6_addr;// указывает на FD_BBMD_Address
    dest_addr.in_port = ((DataLink_IPv6_t*)dl)->FD_BBMD_Address.in_port;//htons(PORT_BACNET_IP);
    return bacnet_ipv6_sendto(dl, &dest_addr, buffer, length);
}
static  int BBMD_result_code(DataLink_t* dl, BACnetAddr_t* dest_addr, uint32_t source_virtual_addr, uint8_t *buffer, uint16_t result)
{
    buffer[0] = BVLL_TYPE_BACNET_IPv6;
    buffer[1] = BVLL_Result;
    *(uint16_t*)&buffer[2] = htons(9);
    vmac_address_store(source_virtual_addr, &buffer[4]);
    *(uint16_t*)&buffer[7] = htons(result);
    return bacnet_ipv6_sendto(dl, dest_addr, buffer, 9);
}
static bool BBMD_FDT_member  (DataLink_t* dl, BACnetAddr_t* addr, uint32_t virtual_addr)
{
    time_t timestamp = time(NULL);//
    FDT_t* entry = ((DataLink_IPv6_t*)dl)->FDT;
    while (entry){
        if (entry->virtual_address == virtual_addr){
            return (timestamp - entry->timestamp) >= entry->ttl;
        }
        entry = entry->next;
    }
    return false;
}
static bool from_local_multicast(DataLink_t* dl, BACnetAddr_t* addr)
{
    return __builtin_memcmp(addr->ipv6_addr, &((DataLink_IPv6_t*)dl)->FD_BBMD_Address.in6_addr,16)==0;
}

static  int FDT_register    (DataLink_t* dl, BACnetAddr_t* addr, uint32_t virtual_addr, uint16_t ttl)
{
    time_t timestamp = time(NULL);//
    FDT_t* entry = ((DataLink_IPv6_t*)dl)->FDT;
    while (entry){
        if (entry->virtual_address == virtual_addr) {
            break;
        }
        entry = entry->next;
    }
    if (entry == NULL) {// ищим просроченную или удаленную запись
        FDT_t* entry = ((DataLink_IPv6_t*)dl)->FDT;
        while (entry){
            if ((timestamp - entry->timestamp) >= entry->ttl) {
                break;
            }
            entry = entry->next;
        }
    }
    if (entry == NULL) {
        entry = g_slice_alloc(sizeof(FDT_t));
        entry->next= ((DataLink_IPv4_t*)dl)->FDT;// foreign device table
        ((DataLink_IPv4_t*)dl)->FDT = entry;
    }
    entry->in6_addr = *(IPv6_Addr_t*)addr->ipv6_addr;
    entry->in_port = addr->in_port;
    entry->virtual_address = virtual_addr;
    entry->ttl = ttl+30;// seconds+grace period
    entry->timestamp = timestamp;//bacnet_timesec();
    // ввыполнить операцию замены атомарно
    return 0;
}
static int FDT_unregister   (DataLink_t *dl, uint32_t virtual_addr, uint8_t *buffer)
{
    IPv6_Addr_t in6_addr = *(IPv6_Addr_t*)&buffer[0];// для NAT traversal GlobalAddress
    uint16_t in_port = *(uint16_t*)&buffer[16];
    FDT_t* entry = ((DataLink_IPv6_t*)dl)->FDT;
    while(entry){
        if (entry->virtual_address == virtual_addr && __builtin_memcmp(&entry->in6_addr, &in6_addr,16)==0 && entry->in_port == in_port) {
            entry->virtual_address = 0;
            return 0;
        }
        entry=entry->next;
    }
    return 1;
}
int bacnet_bvll_indication (DataLink_t* dl_, BACnetAddr_t *source_addr, uint8_t * buffer, size_t size)
{
    DataLink_IPv6_t* dl = (DataLink_IPv6_t*)dl_;
    uint8_t BVLC_type = buffer[0]; // 0x81 - BACnet/IP; 0x82 - BACnet/IPv6
    if (BVLC_type!=BVLL_TYPE_BACNET_IPv6) return -1;
    source_addr->network_number = dl_->network_number;
    uint8_t  BVLC_function = buffer[1];
    uint16_t BVLC_length  = ntohs(*(uint16_t*)&buffer[2]);
    if (BVLC_length != size) return -2;
    printf("BVLL Type: %02X, Function: %02X, Length: %d\n", BVLC_type, BVLC_function, BVLC_length);
    int res = 0;

    //DataUnit_t tr;//={.buffer = buffer+4,.size   = BVLC_length-4};

    switch(BVLC_function) {
    case BVLL_Result: {// U.2.1 BVLC-Result
/*!
Result Code: 2-octets X'0000' Successful completion
        X'0030' Address-Resolution NAK
        X'0060' Virtual-Address-Resolution NAK
        X'0090' Register-Foreign-Device NAK
        X'00A0' Delete-Foreign-Device-Table-Entry NAK
        X'00C0' Distribute-Broadcast-To-Network NAK
*/
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        uint16_t resultCode = ntohs(*(uint16_t*)&buffer[7]);
        printf("\tBVLC-Result: %04X\n", resultCode);
    } break;
    case BVLL_Original_Unicast_NPDU: {// U.2.2 Original-Unicast-NPDU
        printf("\tOriginal-Unicast-NPDU:\n");
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        uint32_t destination_virtual_addr = vmac_address_load(&buffer[7]);
        if (destination_virtual_addr!= dl->vmac_address) {//
            return -4;// discard the message
        }
        vmac_address_set(dl, source_addr, source_virtual_addr);
        res = bacnet_datalink_indication(dl_, source_addr, buffer+10, BVLC_length-10);
    } break;
    case BVLL_Original_Broadcast_NPDU: {// U.2.3 Original-Broadcast-NPDU
/*! Upon receipt of a BVLL Original-Broadcast-NPDU message from the local multicast domain,
a BBMD shall construct a BVLL Forwarded-NPDU message and unicast it to each entry in its BDT.
In addition, the constructed BVLL Forwarded-NPDU message shall be unicast to each foreign device
currently in the BBMD's FDT. */
        printf("\tOriginal-Broadcast-NPDU:\n");
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
// BACnet_IPv6_Multicast_Address
        if (dl->mode == IPv6_Mode_BBMD) {
            BBMD_forwardHdr(source_addr, source_virtual_addr, buffer-18, BVLC_length+18);
            BBMD_distribute(dl_, source_virtual_addr, buffer-18, BVLC_length+18);// не меняет содержимое пакета
        }
        vmac_address_set(dl, source_addr, source_virtual_addr);
        res = bacnet_datalink_indication(dl_, source_addr, buffer+7, BVLC_length-7);
    } break;
    case BVLL_Address_Resolution: {// U.2.4 Address-Resolution
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        uint32_t target_virtual_addr = vmac_address_load(&buffer[7]);
            // добавить адрес source_virtual_addr в таблицу
        if (target_virtual_addr == dl->vmac_address) {// это мой?
            buffer[1] = BVLL_Address_Resolution_ACK;
            vmac_address_store(target_virtual_addr, &buffer[4]);
            vmac_address_store(source_virtual_addr, &buffer[7]);
            bacnet_ipv6_sendto(dl_, source_addr, buffer, 10);
        } else
        if (dl->mode == IPv6_Mode_BBMD) {
/*! Upon receipt of a BVLL Address-Resolution message from the local multicast domain whose destination virtual address is
not itself, the receiving BBMD shall construct and transmit a BVLL Forwarded-Address-Resolution via unicast to each entry
in its BDT as well as to each foreign device in the BBMD's FDT */
            if (from_local_multicast(dl_, source_addr)) {
                BBMD_forwardAR (source_addr, source_virtual_addr, target_virtual_addr, buffer-18);
                BBMD_distribute(dl_, source_virtual_addr, buffer-18, BVLC_length+18);
            } else
            if (BBMD_FDT_member(dl_, source_addr, source_virtual_addr)) {
                BBMD_forwardAR (source_addr, source_virtual_addr, target_virtual_addr, buffer-18);
                BBMD_broadcast (dl_, buffer-18, BVLC_length+18);
                BBMD_distribute(dl_, source_virtual_addr, buffer-18, BVLC_length+18);
            }
        } else {// NACK
        }
    } break;
    case BVLL_Forwarded_Address_Resolution: {// U.2.5 Forwarded-Address-Resolution
        uint32_t original_source_virtual_addr = vmac_address_load(&buffer[4]);
        uint32_t target_virtual_addr = vmac_address_load(&buffer[7]);
        if (target_virtual_addr == dl->vmac_address) {
            // добавить или обновить ARP таблицу
            vmac_address_add(dl, original_source_virtual_addr, &buffer[7]);
        } else {
            if(dl->mode == IPv6_Mode_BBMD /* && BBMD_receiveing_BDT() */){
                //BBMD_distributeFDT()
            }
        }
    } break;
    case BVLL_Virtual_Address_Resolution:{ // U.2.7 Virtual-Address-Resolution
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        buffer[1] = BVLL_Virtual_Address_Resolution_ACK;
        *(uint16_t*)&buffer[2] = htons(10);
        vmac_address_store(dl->vmac_address, &buffer[4]);
        vmac_address_store(source_virtual_addr, &buffer[7]);
        res = bacnet_ipv6_sendto(dl_, source_addr, buffer, 10);
        // есть вариант послать NACK
    } break;
    case BVLL_Virtual_Address_Resolution_ACK: {// U.2.8 Virtual-Address-Resolution-ACK
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        uint32_t target_virtual_addr = vmac_address_load(&buffer[7]);
        if (target_virtual_addr == dl->vmac_address) {
            // добавить или обновить ARP таблицу
        }
    } break;
    case BVLL_Forwarded_NPDU: {// U.2.9 Forwarded-NPDU
/*! Upon receipt of a BVLL Forwarded-NPDU message from a BBMD which is in the receiving BBMD's BDT, a BBMD shall
construct a BVLL Forwarded-NPDU and transmit it via multicast to B/IPv6 devices in the local multicast domain. In
addition, the constructed BVLL Forwarded-NPDU message shall be unicast to each foreign device in the BBMD's FDT. If
the BBMD is unable to transmit the Forwarded-NPDU, or the message was not received from a BBMD which is in the
receiving BBMD’s BDT, no BVLC-Result shall be returned and the message shall be discarded */
        uint32_t original_source_virtual_addr = vmac_address_load(&buffer[4]);
        vmac_address_add(dl, original_source_virtual_addr, &buffer[7]);
        if (dl->mode == IPv6_Mode_BBMD /* && BBMD_can_receive(dl, source_addr, original_source_virtual_addr) */) {
            BBMD_forwardHdr(source_addr, original_source_virtual_addr, buffer, BVLC_length);// -- весьма сомнительная операция
            BBMD_broadcast (dl_, buffer, BVLC_length);
        }
        source_addr->vmac_addr = original_source_virtual_addr; source_addr->len = 3;
        res = bacnet_datalink_indication(dl_, source_addr, buffer+25, BVLC_length-25);
    } break;
    case BVLL_Register_Foreign_Device: {// U.2.10 Register-Foreign-Device
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        uint16_t ttl = ntohs(*(uint16_t*)&buffer[7]);
        uint16_t resultCode = 0;
        if (dl->mode==IPv6_Mode_BBMD && dl->BBMD_Accept_FD_Registrations) {
            res = FDT_register(dl_, source_addr, source_virtual_addr, ttl);
        } else { // отослать NACK
            resultCode = 0x0090;// NACK
        }
        res = BBMD_result_code(dl_, source_addr, source_virtual_addr, buffer-9, resultCode);
    } break;
    case BVLL_Delete_Foreign_Device_Table_Entry: {// U.2.11 Delete-Foreign-Device-Table-Entry
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        uint16_t resultCode = 0;
        if (dl->mode==IPv6_Mode_BBMD && dl->BBMD_Accept_FD_Registrations) {
            res = FDT_unregister(dl_, source_virtual_addr, &buffer[7]);
        } else {
            resultCode = 0x00A0;// NACK
        }
        res = BBMD_result_code(dl_, source_addr, source_virtual_addr, buffer-9, resultCode);
    } break;
    case BVLL_Distribute_Broadcast_To_Network: {// U.2.13 Distribute-Broadcast-To-Network
        uint32_t source_virtual_addr = vmac_address_load(&buffer[4]);
        // добавить и отослать как BVLL_Forwarded_NPDU
        uint16_t resultCode = 0;
        if (dl->mode==IPv6_Mode_BBMD /* && BBMD_from_FD(dl, source_addr, original_source_virtual_addr) */ )
        {// если устройство работает как BBMD, отослать каждому в BDT и в FDT
            BBMD_forwardHdr(source_addr, source_virtual_addr, buffer-18, BVLC_length+18);
            BBMD_broadcast (dl_, buffer-18, BVLC_length+18);
            BBMD_distribute(dl_, source_virtual_addr, buffer-18, BVLC_length+18);
        } else {// NACK
            resultCode = 0x00C0;// NACK
            res = BBMD_result_code(dl_, source_addr, source_virtual_addr, buffer-9, resultCode);
        }
        vmac_address_set(dl, source_addr, source_virtual_addr);
        res = bacnet_datalink_indication(dl_, source_addr, buffer+7, BVLC_length-7);
    } break;
    default:
        res = -3;
        break;
    }
    return res;
}
