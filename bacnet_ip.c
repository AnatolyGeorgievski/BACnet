#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#define closesocket close
#define WSAGetLastError() errno
#define WSAEINTR	EINTR
#define WSAEINPROGRESS	EINPROGRESS
#define WSAEWOULDBLOCK	EWOULDBLOCK
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR	(-1)
#define ioctlsocket	ioctl
#else
#include <winsock2.h>
#include <Ws2tcpip.h>
#define SHUT_RDWR SD_BOTH
#endif

#define BUF_SIZE 1536
#define SVC_NAME "BACnet/IP"

#include <stdint.h>
#include <stdio.h>
#include "bacnet_net.h"
#include "r3_tree.h"

static int bacnet_ipv4_mac_request(DataLink_t* dl, DataUnit_t * transfer);
/*! \brief создает сокет для сеодинения, принимает unicast и broadcast

    \todo ввести параметры IP multicast_ip unicast_ip

 */
DataLink_t* bacnet_ipv4_open(uint16_t port)
{

    SOCKET sock_fd;

    if (port ==0) port = PORT_BACNET_IP;

    //-----------------------------------------------
    // Create a receiver socket to receive datagrams
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd == INVALID_SOCKET) {
        printf("socket failed with error %d\n", WSAGetLastError());
        return NULL;
    }
	int sockopt = 1;
	int res = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof(sockopt));
	if (res<0) {
		return NULL;
	}
	res = setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, (char*)&sockopt, sizeof(sockopt));
	if (res<0) {
		return NULL;
	}
    //-----------------------------------------------
    // Bind the socket to any address and the specified port.
    struct sockaddr_in RecvAddr = {0};
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(PORT_BACNET_IP);//htons(port);
    RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    res = bind(sock_fd, (const struct sockaddr*) & RecvAddr, sizeof (RecvAddr));
    if (res != 0) {
        printf("bind failed with error %d\n", WSAGetLastError());
        return NULL;
    }
    DataLink_t *dl = malloc(sizeof(DataLink_IPv4_t));
    ((DataLink_IPv4_t*)dl)->sock_multicast = sock_fd;
    if (port != PORT_BACNET_IP) {// создать отдельный сокет для получателя
        sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int res = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof(sockopt));
        if (res<0) {
            return NULL;
        }
        res = setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, (char*)&sockopt, sizeof(sockopt));
        if (res<0) {
            return NULL;
        }
        RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        RecvAddr.sin_port = htons(port);
        res = bind(sock_fd, (const struct sockaddr*) &RecvAddr, sizeof (RecvAddr));
        if (res<0) {
            printf("socket failed with error %d\n", WSAGetLastError());
            return NULL;
        }
    }
    ((DataLink_IPv4_t*)dl)->sock_unicast = sock_fd;
    ((DataLink_IPv4_t*)dl)->blacklist = NULL;
    ((DataLink_IPv4_t*)dl)->mode=IPv4_Mode_NORMAL;
    ((DataLink_IPv4_t*)dl)->FD_BBMD_address.in_addr = htonl(INADDR_BROADCAST); /// \todo вставить выделение маски сети и
    ((DataLink_IPv4_t*)dl)->FD_BBMD_address.in_port = htons(PORT_BACNET_IP);
    dl->network_number = LOCAL_NETWORK;
    dl->request = bacnet_ipv4_mac_request;
    dl->routing_table.list = NULL;
    dl->device=NULL;
    return dl;
}

/*! \brief служба перехвата сообщений IPv4
 включая широковещательыне пакеты адресованные 255.255.255.255
 */
int bacnet_ipv4_th(DataLink_t* dl)
{
    SOCKET sock = ((DataLink_IPv4_t*)dl)->sock_unicast;
    SOCKET sock_multicast = ((DataLink_IPv4_t*)dl)->sock_multicast;
    fd_set read_fds;
    const size_t max_pdu = 512;
    uint8_t *pdu = malloc(max_pdu)+20;
    uint8_t *pdu1 = malloc(max_pdu)+20;
    BACnetAddr_t source_addr;
    struct timeval select_timeout = {.tv_sec=10, .tv_usec = 0};
    struct sockaddr_in sin = { 0 };
    int sin_len = sizeof(sin);
    int res =0;
    while(1){
        FD_ZERO(&read_fds);
         if (sock!=sock_multicast) FD_SET(sock_multicast, &read_fds);
        FD_SET(sock, &read_fds);
        int maxfd = sock;//max(sock, sock_multicast);
        /* see if there is a packet for us */
        if (select(maxfd + 1, &read_fds, NULL, NULL, &select_timeout) > 0)
        {
            if (FD_ISSET(sock, &read_fds))
            {
                res = recvfrom(sock, (char *) &pdu[0], max_pdu-20, 0, (struct sockaddr *) &sin, &sin_len);
                if (res>0) {
                    source_addr.in_addr = sin.sin_addr.s_addr;
                    source_addr.in_port = sin.sin_port;
                    source_addr.len = 6;
                    source_addr.network_number = dl->network_number; // локальная сеть
                    printf("IPv4: Done ..(%d)\n", res);
                    bacnet_bvlc_indication(dl, &source_addr, pdu, res);
                }
            }
            if (sock!=sock_multicast && FD_ISSET(sock_multicast, &read_fds)){
                res = recvfrom(sock_multicast, (char *) &pdu1[0], max_pdu, 0, (struct sockaddr *) &sin, &sin_len);
                if (res>0) {
                    source_addr.in_addr = sin.sin_addr.s_addr;
                    source_addr.in_port = sin.sin_port;
                    source_addr.len = 6;
                    source_addr.network_number = dl->network_number; // локальная сеть
                    printf("IPv4: Multicast Done ..(%d)\n", res);
                    bacnet_bvlc_indication(dl, &source_addr, pdu1, res);
                }
            }
            if (0){//printf("IPv4: Keep-alive\n", );
                if (((DataLink_IPv4_t*)dl)->mode == IP_Mode_FOREIGN) {
                    // слать регистрацию по таймауту
                }
            }
        }
    }
    return res;
}
int bacnet_ip_address(BACnetAddr_t* bac_addr, char* address, uint16_t port)
{
    struct hostent *host_ent;
    if ((host_ent=gethostbyname(address)) == NULL) {
        return (-1);
    }
//    uint8_t *mac = g_slice_alloc(8);
    if (bac_addr) {
        bac_addr->network_number = 0;// local
        bac_addr->len = 6;
        bac_addr->in_addr = *(uint32_t*)host_ent->h_addr;
        bac_addr->in_port = htons(port);
    }
    return 0;
}
/*! \brief запросы на передачу данных через BACnet/IP UDP
 */
#if 0
static int bacnet_ipv4_mac_request(DataLink_t* dl, DataUnit_t * transfer)
{
    uint8_t *pdu = transfer->buffer - 4;
    *pdu++ = BVLL_TYPE_BACNET_IP;// IPv4
//если устройств настроено как Foreign device, то отсылка идет черех другие команды
    struct sockaddr_in dest_addr = {.sin_family = AF_INET};
    if (transfer->destination == NULL) {
        *pdu++ = BVLC_Original_Broadcast_NPDU;
        dest_addr.sin_addr.s_addr = dl->broadcast.in_addr;
        dest_addr.sin_port = htons(PORT_BACNET_IP);
    }
    else {
        *pdu++ = BVLC_Original_Unicast_NPDU;
        uint8_t *mac = transfer->destination;
        dest_addr.sin_addr.s_addr = *(uint32_t*)mac;// тут плохо
        dest_addr.sin_port = *(uint16_t*)&mac[sizeof(void*)];
    }
    *(uint16_t*)pdu = htons(transfer->size+4);
    SOCKET sock = (SOCKET)(uintptr_t)dl->handle;
    int res = sendto(sock, (char *) transfer->buffer-4, transfer->size+4, 0,
        (const struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in));
    if (res!= transfer->size+4) {
            printf("sendto() sent a different number of bytes than expected");
    }
    return res;
}
#endif // 0
static int bacnet_ipv4_mac_request(DataLink_t* dl_, DataUnit_t * transfer)
{
    DataLink_IPv4_t *dl = (DataLink_IPv4_t *)dl_;
    uint8_t *pdu = transfer->buffer - 4;
    *pdu++ = BVLL_TYPE_BACNET_IP;// IPv4
//если устройств настроено как Foreign device, то отсылка идет черех другие команды
    struct sockaddr_in dest_addr = {.sin_family = AF_INET};
    SOCKET sock = dl->sock_unicast;
    if (transfer->destination == NULL) {
        *pdu++ = (dl->mode == IP_Mode_FOREIGN)? BVLC_Distribute_Broadcast_To_Network: BVLC_Original_Broadcast_NPDU;
        dest_addr.sin_addr.s_addr = dl->FD_BBMD_address.in_addr;
        dest_addr.sin_port        = dl->FD_BBMD_address.in_port;
    }
    else {
        *pdu++ = BVLC_Original_Unicast_NPDU;
        uint8_t *mac = transfer->destination;
        dest_addr.sin_addr.s_addr = *(uint32_t*)mac;// тут плохо
        dest_addr.sin_port = *(uint16_t*)&mac[sizeof(void*)];
    }
    *(uint16_t*)pdu = htons(transfer->size+4);
    int res = sendto(sock, (char *) transfer->buffer-4, transfer->size+4, 0,
        (const struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in));
    if (res!= transfer->size+4) {
            printf("sendto() sent a different number of bytes than expected (%d)\n", res);
    } else {
//        transfer->status = 0;// Complete, завершение будет когда все броадкасты отошлют.
    }
    return res;
}


int bacnet_ipv6_sendto(DataLink_t* dl, const BACnetAddr_t* destination_addr, const uint8_t *buffer, size_t size)
{
    SOCKET sock = (SOCKET)(uintptr_t)dl->handle;
    struct sockaddr_in6 dest_addr = {0};
    dest_addr.sin6_family = AF_INET6;
    __builtin_memcpy(&dest_addr.sin6_addr, destination_addr->ipv6_addr, 16);
    dest_addr.sin6_port = destination_addr->in_port;//htons(PORT_BACNET_IP);/// взять в другом месте

    int res = sendto(sock, (const char *) buffer, size, 0,
        (const struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in6));
    if (res!= size ) {
            printf("sendto() sent a different number of bytes than expected");
    }
    return res;
}
int bacnet_ip_sendto(DataLink_t* dl, const BACnetAddr_t* destination_addr, const uint8_t *buffer, size_t size)
{
    SOCKET sock = ((DataLink_IPv4_t*)dl)->sock_unicast;
    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = destination_addr->in_addr;
    dest_addr.sin_port = destination_addr->in_port;//htons(PORT_BACNET_IP);/// взять в другом месте
    int res = sendto(sock, (const char *) buffer, size, 0,
        (const struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in));
    if (res!= size ) {
            printf("sendto() sent a different number of bytes than expected");
    }
    return res;
}
/*! \brief запросы на передачу данных через BACnet/IPv6 UDP
 */
int bacnet_ipv6_mac_request(DataLink_t* dl_, DataUnit_t * transfer )
{       /* number of bytes of data */
//    DataLink_IPv6_t *dl = (DataLink_IPv6_t *)dl_;
    uint8_t *pdu = transfer->buffer - 4;
    *pdu++ = 0x82;// IPv6
//    struct _udp_ctx* ctx = handle;
    struct sockaddr_in6 dest_addr = {0};
    dest_addr.sin6_family = AF_INET6;
    if (transfer->destination == NULL) {
        *pdu++ = BVLL_Original_Broadcast_NPDU;
/// \todo        __builtin_memcpy(&dest_addr.sin6_addr, dl->broadcast.ipv6_addr, 16);// multicast
        dest_addr.sin6_port = htons(PORT_BACNET_IP);
    }
    else {
        *pdu++ = BVLL_Original_Unicast_NPDU;
        __builtin_memcpy(&dest_addr.sin6_addr, transfer->destination, 16);// 128 бит
        dest_addr.sin6_port = htons(PORT_BACNET_IP);//*(uint16_t*)&dest->mac[sizeof(void*)];
    }
    *(uint16_t*)pdu = htons(transfer->size+4);
    SOCKET sock = (SOCKET)(uintptr_t)dl_->handle;
    int res = sendto(sock, (char *) transfer->buffer, transfer->size, 0,
        (const struct sockaddr *) &dest_addr, sizeof(struct sockaddr_in6));
    if (res!= transfer->size ) {
            printf("sendto() sent a different number of bytes than expected");
    }
    return res;
}

void __attribute__((constructor)) _init()
{    // Initialize Winsock
    static WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        printf("WSAStartup failed with error %d\n", iResult);
    }
}
static void __attribute__((destructor)) _fini ()
{    // Clean up and exit.
    WSACleanup();
}

#include <iphlpapi.h>

void ip6_address_Enumerate(uint32_t ipAddress)
{
//    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG family = AF_UNSPEC;

//    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
//    ULONG Iterations = 0;
    int i;

    outBufLen = sizeof(IP_ADAPTER_ADDRESSES)*4;
    pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
    dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
        dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
    }
    if (dwRetVal == NO_ERROR) {
        // If successful, output some information from the data we received
        PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            printf("\tLength of the IP_ADAPTER_ADDRESS struct: %ld\n",
                   pCurrAddresses->Length);
            printf("\tIfIndex (IPv4 interface): %lu\n", pCurrAddresses->IfIndex);
            printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);

            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
            if (pUnicast != NULL) {
                for (i = 0; pUnicast != NULL; i++) {
                    printf("IP%s ", (pUnicast->Address.lpSockaddr->sa_family==AF_INET6)?"v6": "v4");
                    pUnicast = pUnicast->Next;
                }
                printf("\tNumber of Unicast Addresses: %d\n", i);
            } else
                printf("\tNo Unicast Addresses\n");

            PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = pCurrAddresses->FirstAnycastAddress;
            if (pAnycast) {
                for (i = 0; pAnycast != NULL; i++)
                    pAnycast = pAnycast->Next;
                printf("\tNumber of Anycast Addresses: %d\n", i);
            } else
                printf("\tNo Anycast Addresses\n");

            PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = pCurrAddresses->FirstMulticastAddress;
            if (pMulticast) {
                for (i = 0; pMulticast != NULL; i++)
                    pMulticast = pMulticast->Next;
                printf("\tNumber of Multicast Addresses: %d\n", i);
            } else
                printf("\tNo Multicast Addresses\n");

            IP_ADAPTER_DNS_SERVER_ADDRESS* pDnServer = pCurrAddresses->FirstDnsServerAddress;
            if (pDnServer) {
                for (i = 0; pDnServer != NULL; i++)
                    pDnServer = pDnServer->Next;
                printf("\tNumber of DNS Server Addresses: %d\n", i);
            } else
                printf("\tNo DNS Server Addresses\n");

            wprintf(L"\tDNS Suffix: %s\n", pCurrAddresses->DnsSuffix);
            wprintf(L"\tDescription: %s\n", pCurrAddresses->Description);
            wprintf(L"\tFriendly name: %s\n", pCurrAddresses->FriendlyName);

            if (pCurrAddresses->PhysicalAddressLength != 0) {
                printf("\tPhysical address: ");
                for (i = 0; i < (int) pCurrAddresses->PhysicalAddressLength;
                     i++) {
                    if (i == (pCurrAddresses->PhysicalAddressLength - 1))
                        printf("%.2X\n",
                               (int) pCurrAddresses->PhysicalAddress[i]);
                    else
                        printf("%.2X-",
                               (int) pCurrAddresses->PhysicalAddress[i]);
                }
            }
            printf("\tFlags: %ld\n", pCurrAddresses->Flags);
            printf("\tMtu: %lu\n", pCurrAddresses->Mtu);
            printf("\tIfType: %ld\n", pCurrAddresses->IfType);
            printf("\tOperStatus: %d\n", pCurrAddresses->OperStatus);
            printf("\tIpv6IfIndex (IPv6 interface): %lu\n",
                   pCurrAddresses->Ipv6IfIndex);
            printf("\tZoneIndices (hex): ");
            for (i = 0; i < 16; i++)
                printf("%lx ", pCurrAddresses->ZoneIndices[i]);
            printf("\n");
#if 0
            printf("\tTransmit link speed: %I64u\n", pCurrAddresses->TransmitLinkSpeed);
            printf("\tReceive link speed: %I64u\n", pCurrAddresses->ReceiveLinkSpeed);
#endif // 0
            IP_ADAPTER_PREFIX * pPrefix = pCurrAddresses->FirstPrefix;
            if (pPrefix) {
                for (i = 0; pPrefix != NULL; i++)
                    pPrefix = pPrefix->Next;
                printf("\tNumber of IP Adapter Prefix entries: %d\n", i);
            } else
                printf("\tNumber of IP Adapter Prefix entries: 0\n");

            printf("\n");

            pCurrAddresses = pCurrAddresses->Next;
        }
    }
}
void ip_address_Enumerate(uint32_t ipAddress)
{
    MIB_IPADDRTABLE  *pIPAddrTable;
    DWORD            dwSize = 0;
    DWORD            dwRetVal;

    pIPAddrTable = (MIB_IPADDRTABLE*) malloc( sizeof(MIB_IPADDRTABLE) );
    if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
        free( pIPAddrTable );
        pIPAddrTable = (MIB_IPADDRTABLE *) malloc ( dwSize );
    }
    if ( (dwRetVal = GetIpAddrTable( pIPAddrTable, &dwSize, 0 )) != NO_ERROR ) {
        printf("GetIpAddrTable call failed with %ld\n", dwRetVal);
    }

    printf("\tNum Entries: %ld\n", pIPAddrTable->dwNumEntries);
    int i;
    for (i=0; i < (int) pIPAddrTable->dwNumEntries; i++) {
    //    printf("IP Address:         %ld\n", pIPAddrTable->table[0].dwAddr);
        printf("IF Index:           %ld\n", pIPAddrTable->table[i].dwIndex);
        printf("IP Address:         %s\n", inet_ntoa(*(struct in_addr*)&pIPAddrTable->table[i].dwAddr));
        printf("IP Mask:            %s\n", inet_ntoa(*(struct in_addr*)&pIPAddrTable->table[i].dwMask));
        printf("Broadcast Addr:     %s (%ld)\n", inet_ntoa(*(struct in_addr*)&pIPAddrTable->table[i].dwBCastAddr), pIPAddrTable->table[i].dwBCastAddr);
        printf("Re-assembly size:   %ld\n", pIPAddrTable->table[i].dwReasmSize);
        printf("Type and State[%d]:", i);
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_PRIMARY)
            printf("\tPrimary IP Address");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_DYNAMIC)
            printf("\tDynamic IP Address");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_DISCONNECTED)
            printf("\tAddress is on disconnected interface");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_DELETED)
            printf("\tAddress is being deleted");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_TRANSIENT)
            printf("\tTransient address");
        printf("\n");
    }
    if (pIPAddrTable)
        free(pIPAddrTable);
}
uint32_t ip_Enumerate2(uint32_t ipAddress)
{
    /* Allocate information for up to 16 NICs */
    IP_ADAPTER_INFO AdapterInfo[16];
    /* Save memory size of buffer */
    DWORD dwBufLen = sizeof(AdapterInfo);
    uint32_t ipMask = INADDR_BROADCAST;
    bool found = false;

    PIP_ADAPTER_INFO pAdapterInfo;

    /* GetAdapterInfo:
       [out] buffer to receive data
       [in] size of receive data buffer */
    DWORD dwStatus = GetAdaptersInfo(AdapterInfo,
        &dwBufLen);
    if (dwStatus == ERROR_SUCCESS) {
        /* Verify return value is valid, no buffer overflow
           Contains pointer to current adapter info */
        pAdapterInfo = AdapterInfo;

        do {
            IP_ADDR_STRING *pIpAddressInfo = &pAdapterInfo->IpAddressList;
            do {
                unsigned long adapterAddress =
                    inet_addr(pIpAddressInfo->IpAddress.String);
                unsigned long adapterMask =
                    inet_addr(pIpAddressInfo->IpMask.String);
                printf("Addr %s mask %s\n", pIpAddressInfo->IpAddress.String, pIpAddressInfo->IpMask.String);
                if (adapterAddress == ipAddress)
                {
                    ipMask = adapterMask;
                    found = true;
                }
                pIpAddressInfo = pIpAddressInfo->Next;
            } while (pIpAddressInfo && !found);
            /* Progress through linked list */
            pAdapterInfo = pAdapterInfo->Next;
            /* Terminate on last adapter */
        } while (pAdapterInfo && !found);
    }

    return ipMask;
}
