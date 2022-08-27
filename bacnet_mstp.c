/*

[RFC 8163] Transmission of IPv6 over Master-Slave/Token-Passing (MS/TP) Networks, May 2017
IPv6 over MS/TP (6LoBAC)

*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmsis_os.h>
#include "bacnet_net.h"
//#include "pio.h"
#include "usart.h"
#include "config.h"
#include "crc.h"
// --------------------

#define WEAK __attribute__ ((weak))
static inline uint16_t ntohs(uint16_t x)
{
    return __builtin_bswap16(x);
	//asm ("rev16 %0, %1" : "=r" (x) : "r" (x));
}
#define NET_BYTE(buf,i) (  (uint32_t)buf[i]  )
#define BACNET_NPDU_LEN_MAX 512


struct _mstp_ctx {
    DataLink_t datalink;// для этого интерфейса
//	DataUnit_t* transfer;
//	DataUnit_t* apdu_transfer;// сделать очередь?
//	void* apdu;
//	osService_t* dl_service;

	uint32_t state;

	DataUnit_t* transfer; // -- входящий трафик для передачи в сеть
	uint8_t*    buffer;

///Slave_Proxy_Enable
///Manual_Slave_Address_Binding
///Auto_Slave_Discovery
///Slave_Address_Binding
//    DataLink_t** network_ports;// ссылки на другие порты таблица ссылок
//    uint8_t Nports;//! число портов, между которыми возможно рутение
//	ttl_datalist_t routing_table;
//	uint16_t network_number;

	uint8_t FrameType;
	uint8_t destination_address;
	uint8_t source_address;
	uint8_t TS;/*!< "This Station," the MAC address of this node. This variable represents the value of the
		MAC_Address property of the node’s Network Port object which represents this MS/TP port. Valid
		values for TS are 0 to 254. The value 255 is used to denote broadcast when used as a destination
		address but is not allowed as a value for TS. */

	uint8_t NS;/*!< "Next Station," the MAC address of the node to which This Station passes the token. If the Next
		Station is unknown, NS shall be equal to TS. */
	uint8_t PS;/*!< "Poll Station," the MAC address of the node to which This Station last sent a Poll For Master. This is
		used during token maintenance. */
	uint8_t RetryCount;/*!< A counter of transmission retries used for Token and Poll For Master transmission */
	uint8_t TokenCount;/*!< The number of tokens received by this node. When this counter reaches the value Npoll, the node
		polls the address range between TS and NS for additional master nodes. TokenCount is set to one at
		the end of the polling process. */
	uint8_t FrameCount;/*!< The number of frames sent by this node during a single token hold. When this counter reaches the
		value Nmax_info_frames, the node must pass the token. */
	bool    SoleMaster;/*!< A Boolean flag set to TRUE by the master machine if this node is the only known master node */
	bool    data_expecting_reply;
};
/*
T_frame_abort -- The minimum time without a DataAvailable or ReceiveError event within a frame before a receiving node
may discard the frame: 60 bit times. (Implementations may use larger values for this timeout, not to exceed
100 milliseconds.)
T_frame_gap -- The maximum idle time a sending node may allow to elapse between octets of a frame the node is
transmitting: 20 bit times.*/
static const uint32_t Treply_delay=250;	/*!< The maximum time a node may wait after reception of a frame that expects a reply before sending the first
			octet of a reply or Reply Postponed frame: 250 milliseconds.*/
static const uint32_t Tusage_timeout=45;/* 15; */ /*!< The maximum time a node may wait after reception of the token or a Poll For Master frame before sending
the first octet of a frame: 15 milliseconds. */
static const uint32_t Tslot =10; /* 10; */		/*! The width of the time slot within which a node may generate a token: 10 milliseconds. */
static const uint32_t Nretry_token = 1;	/*! The number of retries on sending Token: 1*/
static const uint32_t Npoll = 50;/* 50; */ 		/*! The number of tokens received or used before a Poll For Master cycle is executed: 50. */
static const uint8_t  Nmax_master =31;/* 127; */	/*!< The value of Max_Master specifies the highest allowable address for master nodes.  */
static const uint8_t  Nmax_info_frames =16; /*!< of the Max_Info_Frames property of the node's Network Port object
			which represents this MS/TP port. The value of Max_Info_Frames specifies the maximum number of
			information frames the node may send before it must pass the token. */
static const uint32_t T_reply_timeout = 255;/*!< The minimum time without a DataAvailable or ReceiveError event that a node must wait for a station to begin
			replying to a confirmed request: 255 milliseconds. */
static const uint32_t T_postdrive = 15; /*!< The maximum time after the end of the stop bit of the final octet of a transmitted frame before a node must
			disable its EIA-485 driver: 15 bit times */
static const uint32_t Tno_token = 500;	/*!< The time without a DataAvailable or ReceiveError event before declaration of loss of token: 500 milliseconds */
static const uint32_t T_min_octets = 2;	/*!< The minimum number of DataAvailable or ReceiveError events that must be seen by a receiving node in order
to declare the line "active": 4. Нужен другой критерий*/

//static uint8_t buffer[BACNET_NPDU_LEN_MAX];
static uint32_t bacnet_clientID CONFIG_ATTR = 0x1F;// адрес по умолчанию для автоматической настройки
static uint32_t bacnet_netmask  CONFIG_ATTR = 0x1F;// маска сети для поиска соседий методом Poll For Master

static char* MSTP_FrameType_names[] = {
[0]= "Token",
[1]= "Poll For Master",
[2]= "Reply To Poll For Master",
[3]= "Test_Request",
[4]= "Test_Response",
[5]= "BACnet Data Expecting Reply",
[6]= "BACnet Data Not Expecting Reply",
[7]= "Reply Postponed",
};
/*!
Состав пакета:
# Preamble two octet preamble: X'55', X'FF'
# Frame Type one octet
# Destination Address one octet address
# Source Address one octet address
# Length two octets, most significant octet first
# Header CRC one octet
# Data (present only if Length is non-zero)
# Data CRC (present only if Length is non-zero) two octets, least significant octet first
# (pad) (optional) at most one octet of padding: X'FF'

*/
extern uint16_t	bacnet_crc16_update(uint16_t crc, uint8_t val);
extern uint8_t	bacnet_crc8_update (uint8_t  crc, uint8_t val);
/* Кодирование данных методом COBS */
extern size_t cobs_encode(uint8_t * dst, uint8_t * src, size_t length);
extern size_t cobs_decode(uint8_t * dst, uint8_t * src, size_t length);
extern size_t cobs_crc32k(uint8_t * data, size_t length);
extern bool   cobs_crc32k_check(uint8_t * data, size_t length);


extern int bacnet_apdu_server (DataLink_t *dl, uint8_t *buffer, size_t size);

//static int  bacnet_datalink_indication(struct _mstp_ctx* dl, DataUnit_t* tr, bool data_expecting_reply);
static int  bacnet_datalink_release (DataLink_t* dl);
static int  bacnet_mstp_indication(struct _mstp_ctx *dl, uint8_t * buffer, int size);
static void bacnet_mstp_header (struct _mstp_ctx *dl, uint8_t frame_type, bool);
static void bacnet_mstp_request(struct _mstp_ctx *dl, uint8_t frame_type, uint8_t *buffer, size_t data_len);
//static int  bacnet_mstp_service (void const* service_data);
static int  bacnet_mstp_master_service (void * service_data);
//static int  bacnet_mstp_master_fsm(struct _mstp_ctx * dl, uint8_t frame_type, uint32_t signals);
//static int  bacnet_mstp_slave_fsm(struct _mstp_ctx * dl, uint8_t frame_type, uint32_t signals);

#define BROADCAST 0xFF
#define DL_Timeout 1000
/*! \brief состояния мастера */
enum {
	// master & slave
	MSTP_INITIALIZE,
	MSTP_IDLE,
	ANSWER_DATA_REQUEST,
	// мастер
	USE_TOKEN,
	WAIT_FOR_REPLAY,
	DONE_WITH_TOKEN,
	PASS_TOKEN,
	NO_TOKEN,
	POLL_FOR_MASTER,
};

static int bacnet_mstp_mac_request (DataLink_t *datalink, DataUnit_t* tr)
{
//    DataUnit_t* transfer = g_slice_alloc(sizeof(DataUnit_t));
//    *transfer = *tr;
    printf("buffer =%p\n", tr->buffer);
    int i;
    for (i=0; i< tr->size; i++){
        printf("%02X ", tr->buffer[i]);
    }
    printf("\n");
    DataUnitRef(tr);// увеличивает счетчик числа пользователей (процессов)
    osAsyncQueuePut(&datalink->queue, tr);
    return 0;
}
struct _mstp_ctx* bacnet_mstp_init (uint8_t address)
{
	struct _mstp_ctx* dl = malloc(sizeof(struct _mstp_ctx));
//	dl->apdu=NULL;
//	dl->apdu_transfer=NULL;
	dl->TS = address;
	osAsyncQueueInit(&dl->datalink.queue);
	dl->datalink.network_number = LOCAL_NETWORK;// не задан
	dl->datalink.routing_table.list = NULL;//routing_table.list = NULL; // пустая
	dl->datalink.request = bacnet_mstp_mac_request;
	dl->datalink.release = bacnet_datalink_release;
	dl->state = MSTP_INITIALIZE;
	osService_t *svc = osServiceCreate(NULL,bacnet_mstp_master_service, dl);
	dl->datalink.service = svc;
		osServiceTimerCreate(svc, DL_Timeout);// установить время реакции
	dl->datalink.handle = usart_open(address/*! временно!! */, osServiceSignalRef(svc, DL_UNITDATA_indication|DL_UNITDATA_confirm|DL_REPORT_indication));
	// datalink - склеивает сразу несколько уровней: USART0_DRIVER, MSTP_SERVICE, APDU_SERVICE, DEVICE_SERVICE
	// таким способом можно соединять блоки
	dl->buffer = malloc(BACNET_NPDU_LEN_MAX);
//	dl->transfer   = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
	osServiceRun(svc);
    return dl;
}

static int bacnet_mstp_indication (struct _mstp_ctx *dl, uint8_t * buffer, int size)
{

    if (size >= 8 && buffer[0]==0x55 && buffer[1] ==0xFF) {// пропустили приамблу
        buffer+=2; size-=2;
    } else {
		// ошибка, ждем начало пакета, рваный пакет
		// ReceivedInvalidFrame
//		osServiceSetState(MSTP_IDLE);
		return -1;
	}
    dl->FrameType		= buffer[0];
	dl->destination_address = buffer[1];
	dl->source_address		= buffer[2];
    uint16_t data_len = ntohs(*(uint16_t*)&buffer[3]);
    uint8_t hdr_crc = buffer[5];

	int i;
	uint8_t crc8= 0xFF;
	for(i=0;i<5;i++)
		crc8 = bacnet_crc8_update (crc8, buffer[i]);
	crc8 ^= 0xFF;
    if (crc8!=hdr_crc) {
//		osServiceSetState(MSTP_IDLE);
		return -1;// нарушена целостность пакета
	}
    if (data_len > 0) {
		if (dl->FrameType>=32) {//	BACnet Extended Data Expecting Reply
		// предполагается кодирование данных методом COBS
			if (cobs_crc32k_check(buffer+6, data_len-3)){// проверяем CRC и длину пакета
				data_len = cobs_decode(buffer+6, buffer+6, data_len-3);
			} else {
				return -2;// нарушена целостность пакета, пакет отбрасывается или дописывается
			}
		} else {
			uint16_t data_crc = buffer[6+data_len] | (buffer[6+data_len+1]<<8);
			uint16_t crc = 0xFFFF;
			for(i=0;i<data_len;i++)
				crc = bacnet_crc16_update (crc, buffer[i+6]);
			crc ^= 0xFFFF;
            printf("%02X: Data Len: %d Size: %d ", dl->TS, data_len, size);
			printf("CRC: %04X ..%s\n", crc, (crc==data_crc?"ok":"fail"));
			if (crc==data_crc) {// анализ NPDU
//				return 8;// \todo надо определиться как правильно считать длину data_len или size
			} else {
				return -2;// нарушена целостность пакета, пакет отбрасывается или дописывается
			}
		}
    }
	return data_len;
}
/*
00 Token
01 Poll For Master
02 Reply To Poll For Master
03 Test_Request
04 Test_Response
05 BACnet Data Expecting Reply
06 BACnet Data Not Expecting Reply
07 Reply Postponed
32 BACnet Extended Data Expecting Reply
33 BACnet Extended Data Not Expecting Reply
34 IPv6 over MS/TP (LoBAC) Encapsulation
*/
	// \todo анализировать целостность пакета, по длине принятых данных
static int bacnet_mstp_master_service (void * service_data)
{
    // начальное состояние NO_TOKEN -> POLL_FOR_MASTER, PASS_TOKEN
    struct _mstp_ctx* dl = (void*)service_data;
osService_t * self = dl->datalink.service;
osEvent* event = osServiceGetEvent(dl->datalink.service);
uint32_t signals = 0;
if (event->status & osEventSignal)
    signals = event->value.signals;



BACnetAddr_t source_addr;

uint32_t state = dl->state;
uint32_t prev = state;
while(1){
switch (state) {
case NO_TOKEN: {
    if ((event->status & (osEventTimeout|osEventSignal))==osEventTimeout) {// мы получили право на токен, вписались в таймслот
        if (!osAsyncQueueEmpty(&dl->datalink.queue))
            state = USE_TOKEN;
        else
            state = POLL_FOR_MASTER;
        continue;
    } else {// кто-то до нас начал общение
        dl->SoleMaster=false;// мы не одни на линии!
        state = MSTP_IDLE;
        printf("%02X:Activity\n", dl->TS);
        continue;
    }
} break;
case DONE_WITH_TOKEN: {// сюда попадаем только по continue;
    if (dl->TokenCount < Npoll) {// SendToken
        dl->TokenCount++;// сколько раз можно отдать управление NS перед поиском
        dl->destination_address = dl->NS;// если соломастер?
        state = PASS_TOKEN;// Tusage_timeout;
        printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
        osServiceTimerStop(self);
        bacnet_mstp_header(dl, 0x00/* Token Pass */, false);
    }
    else { // искать мастера между TS и NS
        dl->PS = (dl->PS+1) & Nmax_master;
        if (dl->PS == dl->TS) {// SoleMaster
            dl->SoleMaster = true;
            state = USE_TOKEN;
            continue;
        } else
        if (dl->PS != dl->NS) {
            osServiceTimerStop(self);
            dl->destination_address = dl->PS;
            state = POLL_FOR_MASTER;// Tusage_timeout;
            printf("%02X:Poll For Master %02X\n", dl->TS, dl->PS);
            bacnet_mstp_header(dl, 0x01/* Poll For Master */, true);
        } else {// PS == NS
            osServiceTimerStop(self);
            dl->PS = dl->TS;// начнем поиск с TS
            dl->TokenCount=1;
            dl->destination_address = dl->NS;
            state = PASS_TOKEN;// Tusage_timeout;
            printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, false);
        }
    }
} break;
case PASS_TOKEN: {// последовательность из трех состояний confirm, timeout или indication
    if (event->status & osEventTimeout){// 2а) Tusage_timeout
        osServiceTimerStop(self);// можно стопить другим способом
        printf("%02X:PT Timeout\n", dl->TS);
        if (dl->RetryCount<Nretry_token) {// попробовать снова или начать заново сканирование
            dl->RetryCount++;
            dl->destination_address = dl->NS;
            printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, false);
        } else {// RetryCount >= Nretry_token начнем со следующего за NS, или со следующего за
            dl->RetryCount =0;// это единственное место где используется RetryCount?
            dl->PS = (dl->NS+1) & Nmax_master;
            dl->NS = dl->TS;// заново искать
            if (dl->PS == dl->TS) {
                dl->PS = (dl->PS+1) & Nmax_master;
            }
            dl->destination_address = dl->PS;
            state = POLL_FOR_MASTER;// Tusage_timeout);
            bacnet_mstp_header(dl, 0x01/* Poll For Master */, true);
        }
    } else
    if (signals & DL_UNITDATA_confirm) {// 1) команда Token Pass отослана
        signals &= ~DL_UNITDATA_confirm;
        osServiceTimerStart(self, Tusage_timeout);
    }
    if (signals & DL_UNITDATA_indication) {// 2б) начался обмен, пришел пакет его весь город ожидал
        dl->SoleMaster=false;
        //dl->PS = dl->TS;// сканирование начнется заново
        state=MSTP_IDLE;
        continue;
    }
} break;
case POLL_FOR_MASTER: {
    if (event->status & osEventTimeout) { // Tusage_timeout -- не найден
        osServiceTimerStop(self);
        // если известен NS передать токен NS иначе, заняться делом или продолжить сканирование
        uint8_t PS = (dl->PS+1) & Nmax_master;
        if (PS == dl->TS) {// обошли круг не нашли никого, кроме себя
            dl->PS = PS;
            dl->SoleMaster = true;
            dl->FrameCount = 0;// это число раз сколько пакетов
            state = (osAsyncQueueEmpty(&dl->datalink.queue))? DONE_WITH_TOKEN: USE_TOKEN;
            continue;
        } else
        if(dl->TS != dl->NS) {// это условие лучше спрямляет
            dl->TokenCount=0;
            dl->destination_address = dl->NS;
            state = PASS_TOKEN;// Tusage_timeout
            printf("%02X:Pass Token %02X\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, false);
        } else {// продолжить сканирование
            dl->PS = PS;
            dl->destination_address = dl->PS;
            state = POLL_FOR_MASTER;// Tusage_timeout);
            printf("%02X:Poll For Master %02X\n", dl->TS, dl->PS);
            bacnet_mstp_header(dl, 0x01/* Poll For Master */, true);// \todo готовит пакет для отсылки,
        }
    } else
    if (signals & DL_UNITDATA_confirm) { // 1)
//        printf("%02X:PFM Confirm\n", dl->TS);
        signals &= ~DL_UNITDATA_confirm;
        osServiceTimerStart(self, Tusage_timeout);
    }
    if (signals & DL_UNITDATA_indication) {// 2 пришел ответ
//        DataUnit_t transfer = {.buffer = dl->buffer, .size = BACNET_NPDU_LEN_MAX};
        DataUnit_t* tr = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
        int res = bacnet_mstp_indication (dl, tr->buffer, tr->size);
        if (res<0) {
            // битый пакет
            state = MSTP_IDLE;
            osServiceTimerStart(self, Tno_token);
        } else
        //найден, заняться делом или продолжить сканирование, если нет дела
        if ((dl->destination_address == dl->TS) && (dl->FrameType == 0x02)) {// Reply To Poll For Master, ReceivedReplyToPFM
            dl->NS = dl->source_address;
            dl->PS=dl->TS;
            dl->TokenCount=0;
            dl->destination_address = dl->NS;
            state = PASS_TOKEN;
            printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, false);
            signals &= ~DL_UNITDATA_indication;// убрать!
            //osServiceTimerStart(self, Tusage_timeout);
        } else {// пришло что-то странное уступить роль мастера
            printf("%02X:Lost poll for master (Dest=%02X)\n",dl->TS, dl->destination_address);
            state = MSTP_IDLE;
            continue;
        }
    }
} break;
case USE_TOKEN: {// сюда попадаем только по continue
    DataUnit_t* tr;
    if((tr=osAsyncQueueGet(&dl->datalink.queue))!=NULL) {
        dl->FrameCount++;
        bool data_expecting_reply = tr->status & 1;//! \todo см NPDU.control & 0x40 buffer[1]
        if (tr->destination == NULL){//LOCAL_BROADCAST
            dl->destination_address = BROADCAST;
        } else {
            dl->destination_address = (uintptr_t)tr->destination;
        }
        uint8_t FrameType = data_expecting_reply? 0x05:0x06;
        printf("%02X:Transfer mstp use_token len=%d\n", dl->TS, tr->size);

        osServiceTimerStop(self);
        bacnet_mstp_request(dl, FrameType, tr->buffer, tr->size);
        // ждем ответа на буфере отсылки
        state = WAIT_FOR_REPLAY;/// todo для трафика без подтверждения надо ждать подтверждения отправки.
    }
    else {// NothingToSend
        //printf("%02X:DONE_WITH_TOKEN\n", dl->TS);
        state = DONE_WITH_TOKEN;
        continue;
    }
} break;
case WAIT_FOR_REPLAY: {
    osServiceTimerStop(self);
    if ((event->status & osEventTimeout)){// T_reply_timeout
    // сообщение не доставлено, надо сказать об этом приложению
        //N_RELEASE_req(dl);// отпустить буфер, приложения, ответа не будет
        state = DONE_WITH_TOKEN;
        continue;
    } else
    if (event->status & osEventSignal) {
        if (signals & DL_REPORT_indication) {
            //N_RELEASE_req(dl);// отпустить буфер, приложения, ответа не будет
            state = DONE_WITH_TOKEN;
            continue;

        } else
        if (signals & DL_UNITDATA_confirm) {
            //N_RELEASE_req(dl);// отпустили буфер, данные отосланы
            signals &= ~DL_UNITDATA_confirm;
            if (dl->data_expecting_reply) {
                osServiceTimerStart(self, T_reply_timeout);// ждем ответа на буфере отсылки
            } else {
                if ((dl->FrameCount < Nmax_info_frames)) {// есть что слать
                    state = (osAsyncQueueEmpty(&dl->datalink.queue))? DONE_WITH_TOKEN: USE_TOKEN;
                } else { // dl->FrameCount >= Nmax_info_frames
                    state = DONE_WITH_TOKEN;
                }
                continue;
            }
        } else
        if (signals & DL_UNITDATA_indication) { // ReceivedValidFrame, получен ответ
            //DataUnit_t transfer = {.buffer = dl->buffer, .size = BACNET_NPDU_LEN_MAX};
            DataUnit_t* tr = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
            int res = bacnet_mstp_indication(dl, tr->buffer, tr->size);
            if (res<0) {
                // битый пакет
                state = MSTP_IDLE;
                osServiceTimerStart(self, Tno_token);
            } else
            if (dl->destination_address == dl->TS) {// адресован нам
                DataUnit_t* tr = dl->transfer;
                switch (dl->FrameType) {
                case  6:// BACnet Data Not Expecting Reply,
                case 33:{// BACnet Extended Data Not Expecting Reply
                    source_addr.len=1, source_addr.mac[0] = dl->source_address;
                    tr->buffer+=8; tr->size = res;
                    bacnet_datalink_indication(&dl->datalink, &source_addr, tr/*, false*/);
                    state = DONE_WITH_TOKEN;
                } break;
                case  4:// Test_Response,
                    // хоти считать доступность? зачем искали?
                case  7:{// Reply Postponed
                    state = DONE_WITH_TOKEN;
                    /// \todo сразу перейти?
                } break;
                default: // ReceivedUnexpectedFrame
                    state = MSTP_IDLE;
                    continue;
                }
            }
            else {// ReceivedUnexpectedFrame
                state = MSTP_IDLE;// + DL_UNITDATA_indication
                continue;
            }
        }
    }
} break;

case MSTP_IDLE: {
    if ((event->status & (osEventTimeout|osEventSignal))==osEventTimeout) {// LostToken
        printf("%02X: NO_TOKEN Tout=%d\n", dl->TS, (Tslot*dl->TS));
        dl->PS = dl->TS;
        state = NO_TOKEN;
        osServiceTimerStart(self,/* Tno_token+*/(Tslot*dl->TS));// добавить довесок для устойчивости?
    } else
    if (signals & DL_REPORT_indication){// ReceivedInvalidFrame
        printf("%02X:DL_REPORT_indication -- invalid frame\n", dl->TS);
        // перезарядить
//        dl->transfer = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
    } else
    if (signals & DL_UNITDATA_indication) {// пришел запрос
        signals &= ~DL_UNITDATA_indication;
//        DataUnit_t transfer = {.buffer = dl->buffer, .size = BACNET_NPDU_LEN_MAX};
        DataUnit_t* tr = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);// возвращает заполненную структуру
        int res = bacnet_mstp_indication (dl, tr->buffer, tr->size);
        if (res<0) {
            // битый пакет
        } else
        if ((dl->destination_address == dl->TS) || (dl->destination_address == BROADCAST)) {
			switch(dl->FrameType) {
			case 0x00:// Token pass, The Token frame is used to pass network mastership to the destination node.
				if (dl->destination_address == dl->TS) {// ReceivedToken
					dl->FrameCount =0; dl->SoleMaster = false;
					state = (osAsyncQueueEmpty(&dl->datalink.queue))? DONE_WITH_TOKEN: USE_TOKEN;
					continue;
				} else { // ReceivedUnwantedFrame
//					dl->transfer = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
				}
				break;
			case 0x01:// Poll for Master
/* The Poll For Master frame is transmitted by master nodes during configuration and periodically during normal network
operation. It is used to discover the presence of other master nodes on the network and to determine a successor node in the token ring. */
				if (dl->destination_address == dl->TS) {// ReceivedPFM
					dl->destination_address = dl->source_address;
					printf("%02X:Reply to Poll For Master\n", dl->TS);
					bacnet_mstp_header(dl, 0x02, false);// Reply To Poll for Master
				} else {// очищаем и ждем ответа
				}
				break;
			case 0x03:// Test_Request
				// надо проверить целостность вернуть все что пришло запросом
				if (dl->destination_address == dl->TS) {
					dl->destination_address = dl->source_address;
					bacnet_mstp_header(dl, 0x04/* Test_Response */, false);// data_len?
				} else {// очищаем все в таблице, ждем отклика
				}
				break;
			case 32:// BACnet Extended Data Expecting Reply
			case 0x05:// BACnet Data Expecting Reply
				if (dl->destination_address == dl->TS) {// ReceivedDataNeedingReply
                    tr->buffer+=8, tr->size = res;
					bacnet_datalink_indication(&dl->datalink, &source_addr, tr/*, true*/);
					state = ANSWER_DATA_REQUEST;
					osServiceTimerStart(self, Treply_delay);
				} else {// BroadcastDataNeedingReply наш пакет, но отвечать не надо
                    tr->buffer+=8, tr->size = res;
					bacnet_datalink_indication(&dl->datalink, &source_addr, tr/*, false*/);
				}
				break;
			case 33:// BACnet Extended Data Not Expecting Reply
			case 0x06:// BACnet Data Not Expecting Reply
				if ((dl->destination_address == dl->TS) || (dl->destination_address == BROADCAST)) {// ReceivedDataNoReply
                    tr->buffer+=8, tr->size = res;
					bacnet_datalink_indication(&dl->datalink, &source_addr, tr/* , false */);
				} else {// не наш пакет, отбрасываем
				}
				break;
            case 34:// IPv6 over MS/TP (LoBAC) Encapsulation RFC8163, Encoded Data (2 - 1506 octets)
                break;
			default:
				break;
			}
        } else {
            //dl->transfer = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
        }
        //osServiceTimerStart(self, Tno_token);
    }
    if (signals & DL_UNITDATA_confirm) {
        signals &= ~DL_UNITDATA_confirm;
        //printf("%02d: IDLE confirm\n", dl->TS);
        //osServiceTimerStart(self, Tno_token);
    }
    if (state == MSTP_IDLE) osServiceTimerStart(self, Tno_token);
} break;
case MSTP_INITIALIZE:{	// 9.5.6.1 INITIALIZE
    dl->NS = dl->TS;//  (indicating that the next station is unknown)
    dl->PS = dl->TS;// сканирование адресов выполняется между TS и NS
    dl->FrameCount = 0;
    dl->RetryCount = 0;
    dl->TokenCount = Npoll;// (thus causing a Poll For Master to be sent when this node first receives the token),
    dl->SoleMaster = false;
    state = MSTP_IDLE;// Tno_token;// или просто провалиться ниже, в состояние MSTP_IDLE
        /// зарядить на прием
    printf("%02X:Initialize\n", dl->TS);
    osServiceTimerStart(self, Tno_token);// как сделать чтобы все включились и не подрались
} break; // должны прямиком попасть в IDLE
case ANSWER_DATA_REQUEST:{	// 9.5.6.9 ANSWER_DATA_REQUEST, Treply_delay
/* The ANSWER_DATA_REQUEST state is entered when a BACnet Data Expecting Reply, BACnet Extended Data Expecting
Reply, a Test_Request, or a FrameType known to this node that expects a reply is received */
    if (event->status & osEventTimeout) {
        state = MSTP_IDLE;
        /// зарядить на прием
    } else
    if (signals & DL_RELEASE_request){// DeferredReply
        // then an immediate reply is not possible. Any reply shall wait until this node receives the token.
        dl->destination_address = dl->source_address; // -- заранее
        state = MSTP_IDLE;
        bacnet_mstp_header(dl, 0x07/* Reply Postponed */, false);
    } else
    if (signals & DL_UNITDATA_request) {// Reply
        DataUnit_t* tr = dl->transfer;
        dl->destination_address = dl->source_address; // -- заранее
        state = MSTP_IDLE;
        bacnet_mstp_request(dl, 0x06/* Reply */, tr->buffer, tr->size);// \todo готовит пакет для отсылки,
    }
} break;

}
break;
}
    dl->state = state;
#ifdef _WIN32
    if (signals!=0) {
        printf("%02X:signals %02X/%02X %s\n", dl->TS, state, prev, (signals& DL_UNITDATA_confirm)?"confirm":"other");
        _Exit(1);
    }
#endif // __WIN32
    return 0;
}


/*! 9.1.1 DL-UNITDATA.request
This primitive is the service request primitive for the unacknowledged connectionless-mode data transfer service


Receipt of this primitive causes the MS/TP entity to attempt to send the NPDU using unacknowledged connectionless-mode
procedures. BACnet NPDUs less than 502 octets in length are conveyed using BACnet Data Expecting Reply or BACnet Data
Not Expecting Reply frames (see Clause 9.3). BACnet NPDUs between \b 502 and \b 1497 octets in length, inclusive, are conveyed using BACnet Extended Data Expecting Reply or BACnet Extended Data Not Expecting Reply frames.

*/

static void bacnet_mstp_header(struct _mstp_ctx *dl, uint8_t frame_type, bool data_expecting_reply)
{
    uint8_t * buffer = &dl->buffer[BACNET_NPDU_LEN_MAX-8];/// можно использовать маленький буфер 8 байт
	*buffer++= 0x55;// приамбла -- магия
	*buffer++= 0xFF;

	buffer[0] = frame_type;//dl->FrameType;
	buffer[1] = dl->destination_address;
	buffer[2] = dl->TS;
	buffer[3] = 0;
	buffer[4] = 0;
	uint8_t crc8= 0xFF;
	int i;
	for(i=0;i<5;i++)
		crc8 = bacnet_crc8_update (crc8, buffer[i]);
	crc8 ^= 0xFF;
	buffer[5] = crc8;
	usart_send(dl->datalink.handle, buffer-2, 8);
}
static void  bacnet_mstp_request(struct _mstp_ctx *dl, uint8_t frame_type, uint8_t *buffer, size_t data_len)
{
	if (data_len==0) {
	} else
	if (data_len<502) {// 10 байт на шапку и 1 байт на заполнение 0xFF
		uint16_t crc = 0xFFFF;
		size_t i;
		for(i=0;i<data_len;i++)
			crc = bacnet_crc16_update (crc, buffer[i]);
		crc ^= 0xFFFF;
		buffer[data_len] = crc;
		buffer[data_len+1] = crc>>8;
	} else {	// COBS encoded + CRC32K
		data_len = cobs_encode(buffer, buffer, data_len);
		data_len = cobs_crc32k(buffer, data_len);
		data_len-=2;//это число записываем в шапку, корректируем для совместимости с crc16
	}
	buffer-=6;	// компануем пакет -- шапка
	buffer[0] = frame_type;//dl->FrameType;
	buffer[1] = dl->destination_address;
	buffer[2] = dl->TS;//dl->source_address;
	buffer[3] = data_len>>8;
	buffer[4] = data_len;
	uint8_t crc8= 0xFF;
	int i;
	for(i=0;i<5;i++)
		crc8 = bacnet_crc8_update (crc8, buffer[i]);
	crc8 ^= 0xFF;
	buffer[5] = crc8;
	buffer-=2;
	buffer[0]= 0x55;// приамбла -- магия
	buffer[1]= 0xFF;

	data_len += data_len?10:8; // 6 - шапка 2-crc, 2-приамбла
    printf("Transfer mstp request len=%d\n", data_len);

	usart_send(dl->datalink.handle, buffer, data_len);// как узнать что отослалось?
}
/*! 9.1.4 DL-RELEASE.request

This primitive is the service request primitive for releasing a Master Node State Machine from the
ANSWER_DATA_REQUEST state when no reply is available from the higher layers.

This primitive is generated from the network layer to the MS/TP entity to indicate that no reply is available from the higher
layers.
*/
int bacnet_datalink_release(DataLink_t* dl)
{// надо перейти к приему сообещения
	osServiceSignal(dl->service, DL_RELEASE_request);
	return 0;
}
/*! 9.5.7 Slave Node Finite State Machine

The state machine for a slave node is similar to, but considerably simpler than, that for a master node. A slave node shall neither
transmit nor receive segmented messages. If a slave node receives a segmented BACnet-Confirmed-Request-PDU, the node
shall respond with a BACnet-Abort-PDU specifying abort-reason "segmentation not supported."
*/
#if 1
static int bacnet_mstp_slave_fsm(struct _mstp_ctx * dl, uint8_t FrameType, uint32_t signals)
{
//	uint32_t signals = osServiceGetSignals();
    osService_t *self = dl->datalink.service;
    uint32_t state = dl->state;
    osEvent *event = osServiceGetEvent(self);
    BACnetAddr_t source_addr;
    if (event->status == osEventTimeout) {
        osServiceTimerStop(self);
        if (state==ANSWER_DATA_REQUEST) {// T_reply_timeout
//            N_RELEASE_req(dl);// osServiceSignal(dl->apdu_service, N_RELEASE_request);
        } else
        if (state==MSTP_IDLE) {// Tno_token
            /// если мастер надо перейти к поиску токена state = NO_TOKEN
        }
    } else
    if (event->status == osEventSignal){
        /*!
        DL_REPORT_indication, DL_UNITDATA_indication - от порта
        DL_UNITDATA_confirm - от порта, перезарядка буфера,
        DL_RELEASE_request, DL_UNITDATA_request - от приложения
        usart_send() -- к порту, возвращает DL_UNITDATA_confirm, возвращает буфер
        usart_recv() -- к порту, возвращает DL_UNITDATA_indication */
        if (signals & DL_UNITDATA_confirm) {// отсылка данных состоялась
            state = MSTP_IDLE;
        } else
        if (signals & DL_UNITDATA_indication) {// прием данных
			if (dl->destination_address == dl->TS || dl->destination_address == BROADCAST){
				DataUnit_t* tr = dl->transfer;
				int res = bacnet_mstp_indication(dl, tr->buffer, tr->size);
				if (res<0) {
                    state = MSTP_IDLE;
				} else
				switch (dl->FrameType) {
				case  3:// Test_Request
					if (dl->destination_address != BROADCAST) {// ReceivedDataNeedingReply
						bacnet_mstp_header(dl,  0x04/* Test_Response */, /* data_expecting_reply */false);// data_len?
					}
					break;
				case  5:// BACnet Data Expecting Reply
				case 32:// BACnet Extended Data Expecting Reply
                    source_addr.len=1, source_addr.mac[0]= dl->source_address;
                    tr->buffer+=8; tr->size = res;
					if (dl->destination_address != BROADCAST) {// ReceivedDataNeedingReply
						osServiceTimerStart(self, Treply_delay);
						state = ANSWER_DATA_REQUEST;
						bacnet_datalink_indication(&dl->datalink, &source_addr, tr/* , data_expecting_reply =true*/);// буфер принадлежит службе
					}
					else {
						bacnet_datalink_indication(&dl->datalink, &source_addr, tr/*, false*/);// no reply
					}
					break;
				case  6:// BACnet Data Not Expecting Reply
				case 33:// BACnet Extended Data Not Expecting Reply
					{// ReceivedDataNoReply
					    source_addr.len=1, source_addr.mac[0]= dl->source_address;
					    tr->buffer+=8; tr->size = res;
						bacnet_datalink_indication(&dl->datalink, &source_addr, tr/*, false*/);
						/// \todo включить прием данных
					}
					break;
				default:// ReceivedUnwantedFrame
				/* (c) FrameType has a value of Token, Poll For Master, Reply To Poll For Master, Reply Postponed, or a FrameType not known to this node */
                    /// \todo включить прием данных
					break;
				}
			} else {// ReceivedUnwantedFrame
			    /// \todo включить прием данных
			}
        } else
        if (signals & DL_UNITDATA_request) {// получили ответ от приложения
            state = MSTP_IDLE;
			DataUnit_t* tr = dl->transfer;// трансфер принадлежит приложению
			dl->destination_address = dl->source_address;
			bacnet_mstp_request(dl, 0x06, tr->buffer, tr->size);
        } else
        if (signals & DL_RELEASE_request) {// приложение отпустило буфер, зарядили буфер на прием
            state = MSTP_IDLE;
//            dl->transfer = usart_recv(dl->datalink.handle, dl->buffer, BACNET_NPDU_LEN_MAX);
        } else
        if (signals & DL_REPORT_indication) {// от интерфейса, изменение состояния интерфейса
            if (state == MSTP_IDLE) {

            }
        }
    }
    dl->state = state;
	return 0;
}
#endif // 0

