#include <stdio.h>
#include <stdlib.h>
#include "bacnet_net.h"

static inline uint16_t ntohs(uint16_t x)
{
    return __builtin_bswap16(x);
	//asm ("rev16 %0, %1" : "=r" (x) : "r" (x));
}
/*! \brief заполнить заголовок NPDU */
static int bacnet_npdu_hdr(BACnetNPCI* npci, uint8_t *buffer)
{
    int npdu_len = 2;
    if(npci->control & 0x20) npdu_len+=4+npci->DA.len;
    if(npci->control & 0x08) npdu_len+=3+npci->SA.len;
    uint8_t * hdr = buffer-npdu_len;
	*hdr++ = 0x01;// версия протокола
	*hdr++ = npci->control & 0x2F;
    if (npci->control & 0x20) {// Bit 5: Destination specifier
        *hdr++ = npci->DA.network_number>>8;
        *hdr++ = npci->DA.network_number;
        *hdr++ = npci->DA.len;
        if (npci->DA.len) {
            __builtin_memcpy(hdr, npci->DA.mac, npci->DA.len);
            hdr+=npci->DA.len;
        }
    }
    if (npci->control & 0x08) {// Bit 3: Source specifier
        *hdr++ = npci->SA.network_number>>8;
        *hdr++ = npci->SA.network_number;
        *hdr++ = npci->SA.len;
        if (npci->SA.len) {
            __builtin_memcpy(hdr, npci->SA.mac, npci->SA.len);
            hdr+=npci->SA.len;
        }
    }
    if (npci->control & 0x20) {// Bit 5: Destination specifier
        *hdr++ = npci->hop_count;
    }
	return npdu_len;
}
#if 0
/*! \brief дописывает шапку NPDU и ставит в очередь на обработку

*/
int bacnet_datalink_request (DataLink_t* dl, BACnetNPCI* npci, uint8_t * buffer, size_t size)
{
	// добавить шапку NPDU \see 6.2 Network Layer PDU Structure
	size_t npdu_len = 2;// версия и контрольный байт
//	if (npci->DA.network_number != dl->network_number) npci->control |= 0x20;
	if ((npci->control & 0x28)!=0){
		if (npci->control & 0x20) {// Bit 5: Destination specifier
			//uint8_t dlen = npdu[npdu_len];
			if (npci->DA.network_number == dl->network_number) {// попали в локальную сеть, надо отрезать шапку
                npci->control &= ~0x20;// убрали
			} else
                npdu_len += npci->DA.len+4;// байт - hop_count
		}
		if (npci->control & 0x08) {// Bit 3: Source specifier
			//uint8_t slen = npdu[npdu_len];
			npdu_len += npci->SA.len+3;//
		}
	}
	if (npci->control & 0x80) {// контрольные сообщения всегда для локальной сети
		npdu_len ++;
		if (npci->message_type & 0x80) npdu_len ++; // надо добавлять vendor_id, если номер сообщения 0x80 - 0xFF
	}
	// в ответе достаточно ничего не менять для локального трафика.
    buffer -= npdu_len; size+=npdu_len;
    uint8_t * hdr = buffer;
	*hdr++ = 0x01;// версия протокола
	*hdr++ = npci->control;
	if (npci->control & 0x28) {
		if (npci->control & 0x20) {// Bit 5: Destination specifier
            *hdr++ = npci->DA.network_number>>8;
            *hdr++ = npci->DA.network_number;
            *hdr++ = npci->DA.len;
            if (npci->DA.len) {
                __builtin_memcpy(hdr, npci->DA.mac, npci->DA.len);
                hdr+=npci->DA.len;
            }

		}
		if (npci->control & 0x08) {// Bit 3: Source specifier
            *hdr++ = npci->SA.network_number>>8;
            *hdr++ = npci->SA.network_number;
            *hdr++ = npci->SA.len;
            if (npci->SA.len) {
                __builtin_memcpy(hdr, npci->SA.mac, npci->SA.len);
                hdr+=npci->SA.len;
            }
		}
		if (npci->control & 0x20) {// Bit 5: Destination specifier
		    *hdr++ = npci->hop_count;
		}
	}
	if (npci->control & 0x80) {
        *hdr++ = npci->message_type;
	}
#if 1
    DataUnit_t* tr = g_slice_new(DataUnit_t);
    tr->buffer = buffer;
    tr->size = size;
    tr->device_id = npci->DA.vmac_addr;
    tr->status = 3;// ждем или нет ответа? кому принадлежит трансфер
///    dl->queue
    printf("enqueue request\n");
    int i;
    for(i=0;i<size;i++) {
        printf (" %02X", buffer[i]);
    }
    printf (" control=%02X, size=%d\n", npci->control, size);

    g_cond_init(&tr->condition);// os
    g_mutex_lock(&dl->mutex);//этот принадлежит только процессу отсылки C11: int mtx_lock(mtx_t *mtx);
	osAsyncQueuePut(&dl->queue, tr);// в трасфере должен присутсвовать адрес куда слать
    guint64 timestamp = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
    if (g_cond_wait_until (&tr->condition, &dl->mutex, timestamp))
    {// произошло событие
        /* ожидание происходит на семафоре (0=нет события, 1=есть событие) в переменной condition лежит счетчик семафора */
        printf(".. sent\n");
    } else {// таймаут
        printf(".. timeout\n");
        // очистить очередь!!!
    }
    g_mutex_unlock(&dl->mutex);
    g_cond_clear(&tr->condition);

#endif
	return 0;//bacnet_mstp_request(dl, frame_type, buffer, data_len);
}
#endif // 0
/*! 6.2 Network Layer PDU Structure

    6.2.1 Protocol Version Number
Each NPDU shall begin with a single octet that indicates the version number of the BACnet protocol, encoded as an 8-bit
unsigned integer. The present version number of the BACnet protocol is one (1).

    6.2.2 Network Layer Protocol Control Information

The second octet in an NPDU shall be a control octet that indicates the presence or absence of particular NPCI fields. Figure
6-1 shows the order of the NPCI fields in an encoded NPDU. Use of the bits in the control octet is as follows.

Bit 7: 1 indicates that the NSDU conveys a network layer message. Message Type field is present.
       0 indicates that the NSDU contains a BACnet APDU. Message Type field is absent.
Bit 6: Reserved. Shall be zero.
Bit 5: Destination specifier where:
    0 = DNET, DLEN, DADR, and Hop Count absent
    1 = DNET, DLEN, and Hop Count present
    DLEN = 0 denotes broadcast MAC DADR and DADR field is absent
    DLEN > 0 specifies length of DADR field
Bit 4: Reserved. Shall be zero.
Bit 3: Source specifier where:
    0 = SNET, SLEN, and SADR absent
    1 = SNET, SLEN, and SADR present
    SLEN = 0 Invalid
    SLEN > 0 specifies length of SADR field
Bit 2: The value of this bit corresponds to the \b data_expecting_reply parameter in the N-UNITDATA primitives.
    1 indicates that a BACnet-Confirmed-Request-PDU, a segment of a BACnet-ComplexACK-PDU, or a network
    layer message expecting a reply is present.
    0 indicates that other than a BACnet-Confirmed-Request-PDU, a segment of a BACnet-ComplexACK-PDU, or a
    network layer message expecting a reply is present.
Bits 1,0: Network priority where:
    B'11' = Life Safety message
    B'10' = Critical Equipment message
    B'01' = Urgent message
    B'00' = Normal message
*/
/* Network Layer Control Information */
#define BAC_CONTROL_NET		0x80
#define BAC_CONTROL_RES1	0x40
#define BAC_CONTROL_DEST	0x20
#define BAC_CONTROL_RES2	0x10
#define BAC_CONTROL_SRC		0x08
#define BAC_CONTROL_EXPECT	0x04
#define BAC_CONTROL_PRIO_HIGH	0x02
#define BAC_CONTROL_PRIO_LOW	0x01
static size_t bacnet_npdu (BACnetNPCI * npdu, uint8_t * data, size_t size)
{
	uint8_t *ref = data;
    npdu->version = *data++;
    npdu->control = *data++;
    if (npdu->control & 0x20) {// Bit 5: Destination specifier
        npdu->DA.network_number = ntohs(*(uint16_t*)&data[0]); data+=2;
        npdu->DA.len = *data++;
        if (npdu->DA.len!=0) {
            __builtin_memcpy(npdu->DA.mac, data, npdu->DA.len);
            data+=npdu->DA.len;
        }
        //printf("DNET=%04X\n", npdu->dnet);
    } else npdu->DA.len =0;
    if (npdu->control & 0x08) {// Bit 3: Source specifier
        npdu->SA.network_number = ntohs(*(uint16_t*)&data[0]); data+=2;
        npdu->SA.len = *data++;
        if (npdu->SA.len!=0) {
            __builtin_memcpy(npdu->SA.mac, data, npdu->SA.len);
            data+=npdu->SA.len;
        }
        //printf("SNET=%04X\n", npdu->snet);
    }
    if (npdu->control & 0x20) {// Bit 5: Destination specifier
        npdu->hop_count = *data++;
    }
    if (npdu->control & 0x80) {// Bit 7: 1 indicates that the NSDU conveys a network layer message. Message Type field is present.
        //npdu->message_type = *data++;
        if (data[1] & 0x80) {
            //npdu->vendor_id = ntohs(*(uint16_t*)&data[0]); data+=2;
            data+=3;
        } else data+=1;
    }
//    npdu->apdu = data;

    return data-ref;
}

static int bac_weak()
{
    return 0;
}
/* возможно мы не хотим использовать треды, тогда достаточно не включать в компиляющию все что относится к osThread */
#pragma weak bacnet_apdu_service = bac_weak

/*! интерфейс сигнализирует получение данных
    \todo избавиться от трансфера, передавать buffer и size
*/
int bacnet_datalink_indication(DataLink_t* dl, BACnetAddr_t * peer, uint8_t* buffer, size_t size /* DataUnit_t *tr*/  /* , bool data_expecting_reply*/)
{
	int res =0;
    BACnetNPCI npci;
    npci.local = dl->local;
    // ??? npci.response.buffer = malloc(128);//dl->buffer;
    size_t npdu_len = bacnet_npdu(&npci, buffer, size);
    buffer+=npdu_len, size-=npdu_len;
    //! \see Figure 6-12. BACnet Message Routing.
//    NetworkRoutingInfo_t* routing = dl->routing_info;
if (0) {
        int i;
        for(i=0;i<size;i++) {
            printf (" %02X", buffer[i]);
        }
        printf (" control=%02X, size=%d\r\n", npci.control, size);
}
    // результат возвращается на том же буфере?
    if((npci.control & 0x08)==0) {/// Did message come from directly connected network?
        /// Bit 3: Source specifier
        __builtin_memcpy(&npci.SA, peer, sizeof(BACnetAddr_t));
        npci.SA.network_number = dl==NULL?0: dl->network_number;
        npci.control |= 0x08;/// может это ниже делать?
        //printf("-- Add control=%02X SA.len=%d ", npci.control, npci.SA.len);
    }
    if (npci.control & 0x80) {/// Is message a network layer protocol message ?
        /// Bit 7: 1 indicates that the NSDU conveys a network layer message. Message Type field is present.
        /// Process network layer message
#if 0
        int res = bacnet_network_message(dl, &npci, buffer, size);
#endif
        /// Does message need to be relayed?
        if (res==0) return 0;// No
    }
    bool global_broadcast = (npci.control & 0x20) && (npci.DA.network_number==GLOBAL_BROADCAST);
    if((npci.control & (0x80|0x20))==0 || global_broadcast) {/// Is message for this device application layer? адресовано в локальную сеть и не содержит сетевого сообщения
/*! Существует единственная точка через которую обрабатываются запросы к приложению
    В этом месте мы знаем интерфейс, и знаем устройство к которому запрос обращен
*/
//        printf("run apdu service\n");
//		uint8_t control = npci.control;
//		npci.control &= 0x4;// очистить бит ожидания отклика
        res = bacnet_apdu_service(dl, &npci, buffer, size);
		return res;
#if 0
		if ((control & 0x0C)==0x0C) {// Bit 3: Source specifier указан источник, переписать шапку
			npci.control = 0x24;// указать назначение// Bit 5: Destination specifier
			uint8_t * hdr = tr->buffer;
			*hdr++ = 0x01;// версия протокола
			*hdr++ = npci.control;
            *hdr++ = npci->SA.network_number>>8;
            *hdr++ = npci->SA.network_number;
            *hdr++ = npci->SA.len;
            if (npci->SA.len) {
				if (npci->SA.len&4)
					*(uint32_t*)hdr = npci->SA.mac[0];
                __builtin_memcpy(hdr, npci->DA.mac, npci->DA.len);
                hdr+=npci->DA.len;
            }
			*hdr++ = 15;//255 npci->hop_count;
		} else {// в ответе достаточно ничего не менять для локального трафика.
		}
#endif
		return res;
    }
    if((npci.control & 0x08)==0) {/// Did message come from directly connected network?
        /// Bit 3: Source specifier
        /// трансляция адреса??, в рутении принимают участие только известные устройства, хорошо бы предварительно транслировать в VMAC
    }
/*! 9.7.2 Routing of BACnet Messages to MS/TP
When a BACnet network entity issues a DL-UNITDATA.request to a directly connected MS/TP data link, it shall set the
'data_expecting_reply' parameter of the DL-UNITDATA.request equal to the value of the 'data_expecting_reply' parameter of
the network protocol control information of the NPDU, which is transferred in the 'data' parameter of the request */
#if 1
    printf("bacnet_relay\r\n");
	
    res = bacnet_relay(/* dl ->device->network_ports,*/ &npci, buffer, size);

//    Найти интерфейс по network_number, NavService sap_send_recv();
//    доставить пакет.

#else
//	bacnet_datalink;
//
//	npci.control &= ~0x
extern DataLink_t* bacnet_mstp_datalink;
extern DataLink_t* bacnet_ipv4_datalink;
    if (npci.DA.len==6) {// ip
        dl = bacnet_ipv4_datalink;
        printf("route to ipv4\n");
    } else if (npci.DA.len==1){
        dl = bacnet_mstp_datalink;
    }
    if (dl==NULL) {
        printf("no route to host\n");
    } else {
//        npci->
//        res = bacnet_datalink_request(dl, &npci,  buffer, size);
    }
#endif
    return res;// сколько отсылаем
}
/*! \brief инициализация параметров
    \param network_number номер сети, сеть по умолчаню или с неназначенным номером = LOCAL_NETWORK (0)
*/
void bacnet_datalink_init(DataLink_t* dl, uint16_t network_number, DeviceInfo_t* local_device)
{
    dl->network_number=network_number;
#ifdef __linux__    
	g_mutex_init(&dl->mutex);
#endif
    osAsyncQueueInit(&dl->queue);
    dl->local = local_device;
	dl->UNITDATA_request=NULL;
}
int bacnet_relay(/*DataLink_t* ports, */BACnetNPCI *npci, uint8_t * buffer, size_t size)
{
/*
	if (npci==NULL) {
		printf("No NPCI\r\n");
		return -1;
	}
	if (npci->local==NULL) {
		printf("No DeviceInfo\r\n");
		return -1;
	}
	if (npci->local->device==NULL){
		printf("No Device\r\n");
		return -1;
	}
*/
    NetworkPort_t * network_port =npci->local->device->network_ports;
	if (network_port==NULL){
		printf("No Network\r\n");
		return -1;
	} else
    while (network_port) {
        DataLink_t *dl = network_port->datalink;
		if (dl==NULL) {
			//debug("DataLink NULL\r\n");
		} else
		//printf("relay %04X\r\n", dl->network_number);
        if (dl->network_number == npci->DA.network_number) {// в прямой видимости
            // заполнить обратный адрес
            //return bacnet_datalink_request(dl, npci,  buffer, size);
            npci->control &= ~0x20;// очистить флаг Destination
            int npdu_len = bacnet_npdu_hdr(npci, buffer);
            //printf(">> SendRequest to local net %d.%d.%d.%d\r\n", npci->DA.mac[0],npci->DA.mac[1],npci->DA.mac[2],npci->DA.mac[3]);
			if (dl->UNITDATA_request==NULL) {
				//debug("UNITDATA_request NULL\r\n");
			} else {
				//debug("UNITDATA_request\r\n");
				return dl->UNITDATA_request(dl, &npci->DA, buffer-npdu_len, size+npdu_len);
			}
            //break;
        } else {//! \todo проверить правильность заполнения таблицы
			List* list = network_port->routing_table.list;
            while (list) {
				BACnetRouterEntry_t *entry = (BACnetRouterEntry_t *)list->data;
				//debug("routing_table\r\n");
                if (entry->DA.network_number == npci->DA.network_number) {
                    // заполнить обратный адрес

                    npci->control |= 0x20;// установить флаг Destination:Source Address Specifier
                    int npdu_len = bacnet_npdu_hdr(npci, buffer);
                    printf(">> SendRequest\r\n");
                    return dl->UNITDATA_request(dl, &entry->DA,  buffer-npdu_len, size+npdu_len);
                    //break;
                }
                list = list->next;
            }
        }
        network_port = network_port->next;
    }
    printf ("!! --- Network Port port not found --- !!\n");
    return 0;
}

