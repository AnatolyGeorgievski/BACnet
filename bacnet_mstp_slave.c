
#include <cmsis_os.h>
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "config.h"
// --------------------
#include "bacnet_net.h"
#include "r3v2protocol.h"

#define BROADCAST 0xFF
#define BACNET_HEADER_SIZE 6

enum _mstp_fsm {
	IDLE, ANSWER_DATA_REQUEST
};

static void print_hex(const unsigned char *buffer, int len)
{
    int i;
    for (i=0; i< len; i++)
    {
        printf("%02X ", buffer[i]);
        if ((i&0xF) ==0xF) printf("\n");
    }
}
static inline uint16_t ntohs(uint16_t x)
{
    return __builtin_bswap16(x);
	//asm ("rev16 %0, %1" : "=r" (x) : "r" (x));
}
/*! 9.1.1 DL-UNITDATA.request
This primitive is the service request primitive for the unacknowledged connectionless-mode data transfer service


Receipt of this primitive causes the MS/TP entity to attempt to send the NPDU using unacknowledged connectionless-mode
procedures. BACnet NPDUs less than 502 octets in length are conveyed using BACnet Data Expecting Reply or BACnet Data
Not Expecting Reply frames (see Clause 9.3). BACnet NPDUs between \b 502 and \b 1497 octets in length, inclusive, are conveyed using BACnet Extended Data Expecting Reply or BACnet Extended Data Not Expecting Reply frames.

*/

/*! \brief дописать шапку в начало буфера */
static int bacnet_mstp_hdr(uint8_t FrameType, struct _DL_hdr *hdr, uint8_t* buffer, int data_len)
{
	uint8_t * buf = buffer-8;
	*buf++ = 0x55;// приамбула
	*buf++ = 0xFF;
	buf[0] = FrameType;
	buf[1] = hdr->destination_address;
	buf[2] = hdr->source_address; // -- это свойство даталинка, не путать снаружи надо присвоить source_address = dl->TS;
	buf[3] = data_len>>8;
	buf[4] = data_len;
	buf[5] = bacnet_crc8(buf); 
	// todo дописать NPDU в начало буфера
//	*buf++ = 0x01;// version
//	*buf++ = hdr->data_expecting_reply?0x04:0// control

	return 8;
}
/*! \brief Анализ шапки пакета, 
	если шапка с ошибкой, то пропускаем только шапку
	Если пакет адресован не нам, то пропускаем данные от пакета учитывая длину.
 */
static int bacnet_mstp_header_check(struct _DL_hdr *hdr, const uint8_t * buffer, uint16_t *data_len)
{

    //hdr->FrameType		= buffer[0];
	hdr->destination_address = buffer[1];
	hdr->source_address		= buffer[2];
//    *data_len = ntohs(*(uint16_t*)&buffer[3]);
    uint8_t hdr_crc = buffer[5];
	return (bacnet_crc8(buffer)==hdr_crc && hdr->source_address!=0xFF);// нарушена целостность пакета
}
static	void mstp_send(struct _DL_hdr* hdr, uint8_t FrameType, uint8_t *data, size_t data_len) 
{
	//uint8_t FrameType =hdr->FrameType;// hdr->data_expecting_reply?5:6;// BACnet Data Expecting Reply(5) | BACnet Data Not Expecting Reply(6)
	/// выбрать буфер?
	hdr->destination_address = hdr->source_address;// убрать отсюда
	hdr->source_address = hdr->TS;
	bacnet_mstp_hdr(FrameType, hdr, data, data_len);// шапка и преамбула дописать
	// у нас три компоненты: шапка, шапка APDU и ненарезанный буфер APDU
	
	if (data_len>0) {
		if (FrameType>=32) {// кдирование данных может на другом буфере??
			
		} else {
			*(uint16_t*)&data[data_len] = bacnet_crc16(data, data_len);
			data_len+=10;// включая преамбулу 2, CRC16 и шапку 6 байт
		}
	} else {
		data_len = 8;
	}
	if (0){
		printf("MSTP rsp");
		int i;
		for(i=0;i<data_len;i++){
			printf(" %02X", data[i-8]);
		}
		printf("\r\n");
	}
	usart_send(hdr->handler, data-8, data_len);
}	
/*! \brief простой случай 
	Мы не рассматриваем пока процесс переписывания адресов и маршрутизацию.
*/
int bacnet_datalink_ind(struct _DL_hdr* dl_hdr, uint8_t* buffer, size_t size)
{
	int res=0;
	int len=2;
    BACnetNPCI* npci= &dl_hdr->npci;// Network Protocol Control Info
	if (buffer[0]!=0x01) return res;
	uint8_t control =  buffer[1];
	if (control==0x80) {// network message всегда локальные
		
	}
	if ((control&0x20)==0) {// локальный трафик
		if ((control&0x08)!=0) {
			// адрес был длинный
			npci->SA.network_number = ntohs(*(uint16_t*)&buffer[len]);
			npci->SA.len = buffer[len+2];
			switch (npci->SA.len) {
			case 6: 
				npci->SA.in_addr = *(uint32_t*) &buffer[len+3];
				npci->SA.in_port = *(uint16_t*) &buffer[len+7];
				break;
			case 3:
				//npci->SA.vmac_addr = *(uint32_t*) &buffer[len+3];
				break;
			case 1:
				npci->SA.vmac_addr = buffer[len+3];
				break;
			
			}
			len+=3+npci->SA.len;//buffer[4];// при обратной отсылке понадобится добавлять hop_count
		}
		npci->control=control;
		npci->await_response=0;
		res = bacnet_apdu_service((void*)dl_hdr, npci, buffer+len, size-len);
		// два параметра npci.await_response  data_expecting_reply
		dl_hdr->data_expecting_reply=(npci->control&0x04)?1:0;
		dl_hdr->state=(npci->await_response)?ANSWER_DATA_REQUEST:IDLE;
//	printf("oK\r\n");
		if (res>0) {
if (0){
    printf("DL response [%d]= \r\n", res);
    print_hex(buffer+len, res);
    printf("\r\n");
}
			dl_hdr->FrameType = 0x06;// BACnet Data Not Expecting Reply
			uint8_t *buf = buffer+len;
			if (control & 0x08){
				npci->control = 0x20;// destination
				buf -= (npci->SA.len+6); 
				res += (npci->SA.len+6);
				buf[0] = 0x01;
				buf[1] = npci->control;
				*(uint16_t*)&buf[2] = __builtin_bswap16(npci->SA.network_number);
				buf[4] = npci->SA.len;
				switch (npci->SA.len) {
				case 6: 
					*(uint32_t*) &buf[5] = npci->SA.in_addr;
					*(uint16_t*) &buf[9] = npci->SA.in_port;
					break;
				case 3:
					//npci->SA.in_addr = *(uint32_t*) &buffer[len+3];
					break;
				case 1:
					buf[5] = npci->SA.vmac_addr;
					break;
				
				}
				buf[npci->SA.len+5] = 0x0F;// hop_count
			} else {
				buf-=2; res+=2;// размер заголовка NPDU
				buf[0] = 0x01;
				buf[1] = 0x00;//npci->control;
			}
			
			mstp_send(dl_hdr, 0x06, buf, res);
			return res+2;
		}
	} else {
		printf ("drop pkt\r\n");
		print_hex(buffer, size);
		printf("\r\n");
	}
	return res;// не ожидается или ожидается позже 0
}
/*! 9.5.7 Slave Node Finite State Machine

The state machine for a slave node is similar to, but considerably simpler than, that for a master node. A slave node shall neither
transmit nor receive segmented messages. If a slave node receives a segmented BACnet-Confirmed-Request-PDU, the node
shall respond with a BACnet-Abort-PDU specifying abort-reason "segmentation not supported."
*/
static int bacnet_mstp_slave_fsm(uint8_t FrameType, struct _DL_hdr* hdr, uint8_t* buffer, int data_len)
{
	BACnetAddr_t *SA = &hdr->npci.SA;
//	SA->len=1, SA->mac[0]= hdr->source_address;
//	SA->network_number=dl->network_number;
	int resp=-1;
	switch (FrameType) {
#if 0
	case  3:// Test_Request
		if (hdr->destination_address != BROADCAST) {// ReceivedDataNeedingReply
			//hdr->FrameType = 0x04;/* Test_Response */
			resp=data_len;// вернуть все что пришло с запросом
		}
		break;
#endif
	case  5:// BACnet Data Expecting Reply
//	case 32:// BACnet Extended Data Expecting Reply
		if (hdr->destination_address != BROADCAST) {// ReceivedDataNeedingReply
			//osServiceTimerStart(self, Treply_delay);
//			hdr->state = ANSWER_DATA_REQUEST;
//			hdr->data_expecting_reply =true;
			resp = bacnet_datalink_ind(hdr, buffer, data_len);
			if (resp>0) {// отклик содержится на том же буфере
				//hdr->state = ANSWER_DATA_REQUEST;
				//hdr->data_expecting_reply=1;
			} else if (resp==0){// ответ ожидается от приложения
				hdr->state = ANSWER_DATA_REQUEST;
				hdr->data_expecting_reply=0;
			} else {
				hdr->state = IDLE;
			}
		}
		else {
			//npci->data_expecting_reply =false;
			bacnet_datalink_ind(hdr, buffer, data_len /*, false*/);// no reply
		}
		break;
	case  6:// BACnet Data Not Expecting Reply
//	case 33:// BACnet Extended Data Not Expecting Reply
/*		DA.len=1, DA.mac[0]= hdr->destination_address;
		DA.network_number=0; */
		{// ReceivedDataNoReply
			printf("ReceivedDataNoReply\r\n");
			bacnet_datalink_ind(hdr, buffer, data_len);
			hdr->data_expecting_reply=0;
			hdr->state = IDLE;	// включить прием данных
		}
		break;
	case 34:// IPv6 over MS/TP (LoBAC) Encapsulation RFC8163, Encoded Data (2 - 1506 octets)
		break;
	case  0:// Token pass, The Token frame is used to pass network mastership to the destination node.
//		ncpi->master=true;
//		ncpi->next_master = hdr->source_address;// или не менять
		// ответ не требуется
		break;
	case  1:// Poll for Master
/* The Poll For Master frame is transmitted by master nodes during configuration and periodically during normal network
operation. It is used to discover the presence of other master nodes on the network and to determine a successor node in the token ring. */
		printf("MSTP PFM\r\n");
		break;
	case  2:// 
	default:// ReceivedUnwantedFrame
		printf("R3 cmd %02X\r\n", FrameType);
if (0){
    printf("recv [%d]= \n", data_len);
    print_hex(buffer, data_len);
    printf("\n");
}
//		buffer[-1]=FrameType;
		resp = data_len-1;
		hdr->FrameType = R3_cmd_snmp_recv2(FrameType, buffer, &resp); /* Обработка команды R3-SNMP */
//		printf(".. done len=%02X (%02X)\r\n", resp, buffer[0]);
		
//		if (resp>0) resp--;
		hdr->data_expecting_reply=0;
		hdr->state = IDLE;	// включить прием данных

		/* (c) FrameType has a value of Token, Poll For Master, Reply To Poll For Master, Reply Postponed, or a FrameType not known to this node */
		break;
	}
	return resp;
}

#define BACNET_LEN_MAX 256
#define Tno_token 512 // миллисекунд
#define Treplay_delay 5 // миллисекунд
static uint8_t buffer[BACNET_LEN_MAX];
uint32_t clientID_mem CONFIG_ATTR = 0x1F;// адрес по умолчанию для автоматической настройки

#define MSTP_FLAGS (1)
#define MSTP_UNITDATA_indication (1<<MSTP_FLAGS)
#define MSTP_UNITDATA_confirm    (2<<MSTP_FLAGS)
#define MSTP_REPORT_indication   (4<<MSTP_FLAGS)
#ifdef BOARD_BACNET_MSTP_PORT
#define MSTP_PORT BOARD_BACNET_MSTP_PORT
#else 
#define MSTP_PORT 0
#endif

void bacnet_mstp_thr(void const* user_data)
{
	DeviceInfo_t * local_device = (void*)user_data;

	/* запустить службу обработки протокола */
	uint8_t clientID = clientID_mem;
	osEvent event;
	struct _DL_hdr hdr = {.state=IDLE, .TS=clientID};
	hdr.owner = osThreadGetId();
	hdr.handler = usart_open(MSTP_PORT, osSignalRef(hdr.owner, MSTP_FLAGS));
	hdr.npci.local = local_device;
	hdr.npci.response.MTU = 512;
	hdr.npci.response.buffer = malloc(hdr.npci.response.MTU);
	
	printf("BACnet MS/TP slave addr=%02X\r\n", clientID);
// nested

	do {
		osTransfer * transfer = usart_recv(hdr.handler, buffer, BACNET_LEN_MAX);
		uint32_t signal_mask, timeout;// = MSTP_UNITDATA_indication;//1<<1;// используем три флага DL_indication|DL_confirm| DL_report, 
		if (hdr.data_expecting_reply==0 && hdr.state==ANSWER_DATA_REQUEST) {// не ждем потеврждение
			timeout = Treplay_delay;
			signal_mask = MSTP_UNITDATA_confirm|DL_UNITDATA_request|DL_RELEASE_request;
			// может быть другой таймаут
		} else
		if (hdr.data_expecting_reply==1 && hdr.state==ANSWER_DATA_REQUEST) {// ждем потеврждение 
			timeout = Treplay_delay;
			signal_mask = MSTP_UNITDATA_indication|DL_UNITDATA_request|DL_RELEASE_request;
		} else {// в состоянии IDLE
			timeout = Tno_token;
			signal_mask = MSTP_UNITDATA_indication|DL_UNITDATA_request|DL_RELEASE_request;// добавить обработку сетевых ошибок
		}
		signal_mask |= MSTP_UNITDATA_confirm| MSTP_REPORT_indication;
		event = osSignalWait(signal_mask/*1<<1*/, timeout);// используем три флага indication|confirm|report, 
		if (event.status & osEventTimeout){
			if (hdr.state==ANSWER_DATA_REQUEST) {
				printf("DL CannotReply \r\n");
				hdr.state=IDLE;// может отослать postponed
			} else {
				//printf("USART1 Rx Timeout\r\n");
			}
			continue;
		} 
		if (event.status & osEventSignal) {
			uint32_t signals = event.value.signals;
//			printf("signal %x\r\n", signals);
			if (signals & DL_RELEASE_request)  {// 9.1.4 DL-RELEASE.request
				/* This primitive is generated from the network layer to the MS/TP entity to indicate that no reply is available from the higher layers 
				9.1.4.4 Effect on Receipt
				Receipt of this primitive causes the MS/TP Master Node State Machine to leave the ANSWER_DATA_REQUEST state. If a Master Node State Machine is not in the ANSWER_DATA_REQUEST state or a Slave Node State Machine is present, then this primitive shall be ignored
				*/
				hdr.state = IDLE;//answer_data_request = 0;// state
				osSignalClear(hdr.owner,DL_RELEASE_request);
				continue;
			} else
			if (signals & DL_UNITDATA_request) {// 9.1.1 DL-UNITDATA.request
				/* 	This primitive is passed from the network layer to the MS/TP entity to request that a network protocol data unit (NPDU) be sent to one or more remote LSAPs using unacknowledged connectionless-mode procedures */
				//tr = osAsyncQueueGet();
				mstp_send(&hdr, 0x06, hdr.data, hdr.data_len);
				osSignalClear(osThreadGetId(),DL_UNITDATA_request);
				// сформировать пакет размером MTU из ctx->npdu_data и отослать
				// снять или не снимать флаги 'data_expecting_reply' 'await_response'
				continue;
			} else 
			if (signals & MSTP_UNITDATA_confirm){// подтверждение отсылки ACK
				if (0) printf("MSTP_UNITDATA_confirm\r\n");
				osSignalClear(osThreadGetId(),MSTP_UNITDATA_confirm);
				if (hdr.data_expecting_reply==0 && hdr.state==ANSWER_DATA_REQUEST) {// не ждем подтверждение отсылаем несколько пакетов
					if (signals & DL_UNITDATA_request) {// на момент завершения отсылки данные готовы
						mstp_send(&hdr, 0x06, hdr.data, hdr.data_len);
						osSignalClear(osThreadGetId(),MSTP_UNITDATA_confirm|DL_UNITDATA_request);
						hdr.data_expecting_reply=1;// ждем подтверждение
						continue;
					} else {
						// может быть reply postponed
						//mstp_send_reply_postponed(&hdr)
					}
				}
				
			}
			if (signals & MSTP_REPORT_indication) {
				osSignalClear(osThreadGetId(),MSTP_REPORT_indication);// 1<<1);
				/* \todo проверить статус завершения трансфера */
				printf("Rx error = %d\r\n", transfer->status);
				hdr.state=IDLE;
				// выполнить процедуру восстановления
			} else 
			{
				osSignalClear(osThreadGetId(), MSTP_UNITDATA_indication);// 1<<1);
				int len = transfer->size;
				if (len >= 8 && buffer[0]==0x55 && buffer[1] ==0xFF) {// пропустили приамблу
					uint8_t FrameType = buffer[2];
					uint16_t data_len = ntohs(*(uint16_t*)&buffer[5]);
					if (!bacnet_mstp_header_check(&hdr, buffer+2, &data_len)) {// пакет битый
						// dl->error_ctr++;// счетчик ошибок интерфейса
						printf("MSTP hdr error\r\n");
						continue;
					} else
					if (!(hdr.destination_address==hdr.TS || hdr.destination_address==BROADCAST))
					{// пакет адресован не нам, пропустить
if(0) {
	printf("MSTP hdr(%02X): ", hdr.destination_address);
	int i;
	for(i=0;i<8;i++){
		printf(" %02X", buffer[i]);
	}
	printf("\r\n");
}
						continue;
					}
					if (data_len != 0) {// декодировать данные
						if (0/*FrameType>=32*/) {// декодирвование данных может на другом буфере??
							
						} else {
							uint16_t crc = bacnet_crc16(buffer+8, data_len);
							uint16_t data_crc = *(uint16_t*)&buffer[8+data_len];
							if (crc!=data_crc) {// контрольная сумма не сходится, ошибка
								//resp = -1;
								printf("CRC error %04X\r\n", data_crc);
								continue;
							}
						}
						// надо обработать данные
					}
					if (0) printf("MSTP pack (%d)\r\n", FrameType);
if(0) {
	printf("MSTP hdr: ");
	int i;
	for(i=0;i<8;i++){
		printf(" %02X", buffer[i]);
	}
	printf("  ");
	for(i=0;i<data_len;i++){
		printf(" %02X", buffer[i+8]);
	}
	printf("\r\n");
}
					
					uint8_t * data =  buffer+8;
					int resp = bacnet_mstp_slave_fsm(FrameType, &hdr, data, data_len);
					// (1) подтверждение не требуется  (-1)
					// (2) ответ ожидается от приложения 'await_response': зарядить ожидание события или входящего пакета. Два буфера приема
					// (3) ожидается подтверждение на пакет 'data_expecting_reply'
					if (resp >=0){// требуется ответ
						// TODO задействовать асинхронную очередь в режиме мастера tr->hdr, tr->buffer!!!
						//mstp_send(&hdr, data, resp); перенес выше
					} else {
						if(0) printf("MSTP fsm\r\n");
					}
				} else if (len>0){// ошибка формата, запустить процедуру восстановления соедиенния
					printf("BACnet error Len = %d\r\n", len);
					int i;
					for(i=0;i<8;i++){
						printf(" %02X", buffer[i]);
					}
					printf("\r\n");
					continue;

				}
			}
		}
	} while(1);
}

#include "r3v2protocol.h"
/*! \brief Запросить адрес устройства на линии 

	Значние применяется при включении и после перезагрузки
	[0] - адрес устройства
*/
static int radian_ping(uint8_t* buffer, int* length)
{
	buffer[0]=clientID_mem;
	*length = 1;
	return R3_ERR_OK;
}
/*! \brief Изменить адрес устройства на линии 
	\return код завершения 
	[0] - адрес устройства
*/
static int radian_set_address(uint8_t* buffer, int* length)
{
	clientID_mem = buffer[0] & 0x1F;
	printf("new addr =%02x\r\n", clientID_mem);
	*length = 0;
	return R3_ERR_OK;
}

#define R3_RADIAN_SET_ADDRESS 0x40
#define R3_RADIAN_GET_ADDRESS (0x40|0x80)
R3CMD(R3_RADIAN_SET_ADDRESS, radian_set_address); // адрес на линии
R3CMD(R3_RADIAN_GET_ADDRESS, radian_ping);
