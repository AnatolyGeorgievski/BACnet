#include <cmsis_os.h>
#include <config.h>
#include "usart.h"
#include "bacnet_net.h"
#include <stdio.h>
#include <stdlib.h>
#define DBG 1
	extern void ITM_flush();

struct _mstp_ctx {
    DataLink_t datalink;// для этого интерфейса
//	DataUnit_t* transfer;
//	DataUnit_t* apdu_transfer;// сделать очередь?
//	void* apdu;
//	osService_t* dl_service;

	uint32_t state;
	void* handle; // драйвер RS-485
//	DataUnit_t* request;    // сохраняем ссылку на время ожидания отклика от устройства
	DataUnit_t* response;   // входящий трафик для передачи в сеть
	uint8_t*    buffer;     // буфер принадлежит службе
	size_t data_len;// длина отклика

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

#define BACNET_NPDU_LEN_MAX 512
#define BROADCAST 0xFF
/* Настройки протокола
T_frame_abort -- The minimum time without a DataAvailable or ReceiveError event within a frame before a receiving node
may discard the frame: 60 bit times. (Implementations may use larger values for this timeout, not to exceed
100 milliseconds.)
T_frame_gap -- The maximum idle time a sending node may allow to elapse between octets of a frame the node is
transmitting: 20 bit times.*/
#define ms 1000
static const uint32_t Treply_delay=250*ms;	/*!< The maximum time a node may wait after reception of a frame that expects a reply before sending the first
			octet of a reply or Reply Postponed frame: 250 milliseconds.*/
static const uint32_t Tusage_timeout=45*ms;/* 15; */ /*!< The maximum time a node may wait after reception of the token or a Poll For Master frame before sending
the first octet of a frame: 15 milliseconds. */
static const uint32_t Tslot =10*ms; /* 10; */		/*! The width of the time slot within which a node may generate a token: 10 milliseconds. */
static const uint32_t Nretry_token = 1;	/*! The number of retries on sending Token: 1*/
static const uint32_t Npoll = 50;/* 50; */ 		/*! The number of tokens received or used before a Poll For Master cycle is executed: 50. */
static const uint8_t  Nmax_master =15;/* 127; */	/*!< The value of Max_Master specifies the highest allowable address for master nodes.  */
static const uint8_t  Nmax_info_frames =16; /*!< of the Max_Info_Frames property of the node's Network Port object
			which represents this MS/TP port. The value of Max_Info_Frames specifies the maximum number of
			information frames the node may send before it must pass the token. */
static const uint32_t T_reply_timeout = 255*ms;/*!< The minimum time without a DataAvailable or ReceiveError event that a node must wait for a station to begin
			replying to a confirmed request: 255 milliseconds. */
static const uint32_t T_postdrive = 15; /*!< The maximum time after the end of the stop bit of the final octet of a transmitted frame before a node must
			disable its EIA-485 driver: 15 bit times */
static const uint32_t Tno_token = 500*ms;	/*!< The time without a DataAvailable or ReceiveError event before declaration of loss of token: 500 milliseconds */
static const uint32_t T_min_octets = 2;	/*!< The minimum number of DataAvailable or ReceiveError events that must be seen by a receiving node in order
to declare the line "active": 4. Нужен другой критерий*/
/*! \brief состояния мастера */
enum _MSTP_Master_state {
	// master & slave
	MSTP_INITIALIZE,
	MSTP_IDLE,
	ANSWER_DATA_REQUEST,
	// мастер
	USE_TOKEN,
	WAIT_FOR_REPLY,
	DONE_WITH_TOKEN,
	PASS_TOKEN,
	NO_TOKEN,
	POLL_FOR_MASTER,
};

static inline uint16_t ntohs(uint16_t x) {   return __builtin_bswap16(x); }

// Набор затычек Weak позволяет отлажить под Windows
#pragma weak usart_recv = _weak_recv
#pragma weak rs485_send = _weak_send
#pragma weak rs485_init = _weak_init
extern void *rs485_init(const char* device_name, const char* port_settings);
static void *_weak_init(const char* device_name, const char* port_settings) { return NULL; }
extern DataUnit_t* usart_recv(void* hdl, void * buffer, size_t max_length);
static DataUnit_t* _weak_recv(void* hdl, void * buffer, size_t max_length){ return NULL; }
extern void usart_send(void* hdl, void * buffer, size_t length);
static int _weak_send(void* hdl, uint8_t * buffer, size_t length) { return osErrorResource; }
/* возможно мы не хотим использовать треды, тогда достаточно не включать в компиляющию все что относится к osThread */

/*! \brief разбор входящих пакетов */
static int bacnet_mstp_indication (struct _mstp_ctx *dl, uint8_t * buffer, int size)
{

    if (size >= 8 && buffer[0]==0x55 && buffer[1] ==0xFF) {// пропустили приамблу
        buffer+=2; size-=2;
    } else {
		// ошибка, ждем начало пакета, рваный пакет
		printf("ReceivedInvalidFrame\r\n");
//		osServiceSetState(MSTP_IDLE);
		return -1;
	}
    dl->FrameType		= buffer[0];
	dl->destination_address = buffer[1];
	dl->source_address		= buffer[2];
    uint16_t data_len = ntohs(*(uint16_t*)&buffer[3]);
    uint8_t hdr_crc = buffer[5];

	uint8_t crc8= bacnet_crc8(buffer);
    if (crc8!=hdr_crc) {
//		osServiceSetState(MSTP_IDLE);
		return -1;// нарушена целостность пакета
	}
    if (data_len > 0) {
		if (dl->FrameType>=32) {//	BACnet Extended Data
		// предполагается кодирование данных методом COBS
#if 0
			if (cobs_crc32k_check(buffer+6, data_len-3)){// проверяем CRC и длину пакета
				data_len = cobs_decode(buffer+6, buffer+6, data_len-3);
			} else 
#endif
			{
				return -2;// нарушена целостность пакета, пакет отбрасывается или дописывается
			}
			
		} else {
			uint16_t data_crc = buffer[6+data_len] | (buffer[6+data_len+1]<<8);
			uint16_t crc = bacnet_crc16(buffer+6, data_len);
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
/*! \brief дописать шапку в начало буфера
    буфер уже содержит заголовок NPDU,
    \param указать должен показывать на начало буфера, куда записывается шапка сообщения
*/
static void bacnet_mstp_hdr(uint8_t FrameType, uint8_t DA, uint8_t SA, uint8_t* buf, int data_len)
{
//	uint8_t * buf = buffer-8;
	*buf++ = 0x55;// приамбула
	*buf++ = 0xFF;
	buf[0] = FrameType;
	buf[1] = DA;//dl->destination_address;
	buf[2] = SA;//source_address; // -- это свойство даталинка, не путать снаружи надо присвоить source_address = dl->TS;
	buf[3] = data_len>>8;
	buf[4] = data_len;
	buf[5] = bacnet_crc8(buf);
	// дописать NPDU в начало буфера
//	*buf++ = 0x01;// version
//	*buf++ = hdr->data_expecting_reply?0x04:0// control
}
/*! \brief Анализ шапки пакета,
	если шапка с ошибкой, то пропускаем только шапку
	Если пакет адресован не нам, то пропускаем данные от пакета учитывая длину.
 */
static int bacnet_mstp_hdr_check(struct _mstp_ctx * dl, uint8_t * buf, uint16_t *data_len)
{
//    if (!(buf[0]==0x55 && buf[1]==0xFF)) return 0;
    buf+=2;
    dl->FrameType		    = buf[0];
	dl->destination_address = buf[1];
	dl->source_address		= buf[2];
    *data_len = ntohs(*(uint16_t*)&buf[3]);
    uint8_t hdr_crc = buf[5];
	return (bacnet_crc8(buf)==hdr_crc && dl->source_address!=0xFF);// нарушена целостность пакета
}
static uint8_t hdr_sent_buffer[8];
/*! \brief отослать короткое (сетевое) сообщение состоящее только из шапки */
static void bacnet_mstp_header(struct _mstp_ctx *dl, uint8_t FrameType, uint8_t DA,  bool data_expecting_reply)
{
    uint8_t *buffer = hdr_sent_buffer;//[8];/// можно использовать маленький буфер 8 байт
    bacnet_mstp_hdr(FrameType, DA, dl->TS, buffer, 0);
	usart_send(dl->handle, buffer, 8);
	if (0){
		int i;
		for(i=0;i<8;i++){
			printf(" %02X", buffer[i]);
		}
		printf("\r\n");
	}
}
/*! \brief отослать сообщение с данными */
static int bacnet_mstp_request(struct _mstp_ctx *dl, uint8_t FrameType, uint8_t DA, uint8_t* buffer, int data_len)
{
    bacnet_mstp_hdr(FrameType, DA, dl->TS, buffer-8, data_len);
    if (data_len>0) {
        /// TODO COBS
        *(uint16_t*)&buffer[data_len] = bacnet_crc16(buffer, data_len);
        data_len+=10;
    } else {
        data_len =8;
    }
	usart_send(dl->handle, buffer-8, data_len);
	if (DBG){
		int i;
		for(i=0;i<8;i++){
			printf(" %02X", buffer[i-8]);
		}
		printf(" -");
		for(i=0;i<data_len-8;i++){
			printf(" %02X", buffer[i]);
		}
		printf("\r\n");
	}

	return 0;
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
typedef struct _DataUnit_MSTP DataUnit_MSTP_t;
struct _DataUnit_MSTP {
	uint8_t* buffer;
	uint16_t size;
	uint8_t status;
	uint8_t device_id;
	GCond   condition;
};
	// \todo анализировать целостность пакета, по длине принятых данных
int bacnet_mstp_master_service (osService_t * svc, void * service_data)
{
    // начальное состояние NO_TOKEN -> POLL_FOR_MASTER, PASS_TOKEN
    struct _mstp_ctx* dl = (void*)service_data;
osEvent* event = osServiceGetEvent(svc);
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
        if(DBG) printf("%02X:Activity\n", dl->TS);
        continue;
    }
} break;
case DONE_WITH_TOKEN: {// сюда попадаем только по continue;
    if(DBG) printf("%02X:Done with Token\n", dl->TS);

    if (dl->TokenCount < Npoll) {// SendToken
        dl->TokenCount++;// сколько раз можно отдать управление NS перед поиском
        dl->destination_address = dl->NS;// если соломастер?
        state = PASS_TOKEN;// Tusage_timeout;
        if(DBG) printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
//        osServiceTimerStop(svc);
        bacnet_mstp_header(dl, 0x00/* Token Pass */, dl->NS, false);
        osServiceTimerStart(svc, Tusage_timeout);
    }
    else { // искать мастера между TS и NS
        dl->PS = (dl->PS+1) & Nmax_master;
        if (dl->PS == dl->TS) {// SoleMaster
            dl->SoleMaster = true;
            state = USE_TOKEN;
            continue;
        } else
        if (dl->PS != dl->NS) {
//            osServiceTimerStop(svc);
            dl->destination_address = dl->PS;
            state = POLL_FOR_MASTER;// Tusage_timeout;
            if(DBG) printf("%02X:Poll For Master %02X\n", dl->TS, dl->PS);
            bacnet_mstp_header(dl, 0x01/* Poll For Master */, dl->PS, true);
		
            osServiceTimerStart(svc, Tusage_timeout);
        } else {// PS == NS
//            osServiceTimerStop(svc);
            dl->PS = dl->TS;// начнем поиск с TS
            dl->TokenCount=1;
            dl->destination_address = dl->NS;
            state = PASS_TOKEN;// Tusage_timeout;
            if(DBG) printf("%02X:Token Pass %02X (PS == NS)\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, dl->NS, false);
            osServiceTimerStart(svc, Tusage_timeout);
        }
    }
} break;
case PASS_TOKEN: {// последовательность из трех состояний confirm, timeout или indication
    if (event->status & osEventTimeout){// 2а) Tusage_timeout
        osServiceTimerStop(svc);// можно стопить другим способом
        if(DBG) printf("%02X:PT Timeout\n", dl->TS);
        if (dl->RetryCount<Nretry_token) {// попробовать снова или начать заново сканирование
            dl->RetryCount++;
            dl->destination_address = dl->NS;
            if(DBG) printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, dl->NS, false);
        } else {// RetryCount >= Nretry_token начнем со следующего за NS, или со следующего за
            dl->RetryCount =0;// это единственное место где используется RetryCount?
            dl->PS = (dl->NS+1) & Nmax_master;
            dl->NS = dl->TS;// заново искать
            if (dl->PS == dl->TS) {
                dl->PS = (dl->PS+1) & Nmax_master;
            }
            dl->destination_address = dl->PS;
            state = POLL_FOR_MASTER;// Tusage_timeout);
            bacnet_mstp_header(dl, 0x01/* Poll For Master */, dl->PS, true);
            osServiceTimerStart(svc, Tusage_timeout);
        }
    } else
    if (signals & DL_UNITDATA_confirm) {// 1) команда Token Pass отослана
        signals &= ~DL_UNITDATA_confirm;
        osServiceTimerStart(svc, Tusage_timeout);
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
        osServiceTimerStop(svc);
        // если известен NS передать токен NS иначе, заняться делом или продолжить сканирование
        uint8_t PS = (dl->PS+1) & Nmax_master;
        if (PS == dl->TS) {// обошли круг не нашли никого, кроме себя
            dl->PS = PS;
            dl->SoleMaster = true;
            if(DBG) printf("%02X:SoleMaster condition\n", dl->TS);
            dl->FrameCount = 0;// это число раз сколько пакетов
            state = (osAsyncQueueEmpty(&dl->datalink.queue))? DONE_WITH_TOKEN: USE_TOKEN;
            continue;
        } else
        if (dl->SoleMaster && !osAsyncQueueEmpty(&dl->datalink.queue)) {
            state = USE_TOKEN;
            continue;
        } else
        if(dl->TS != dl->NS) {// это условие лучше спрямляет
            dl->TokenCount=0;
            dl->destination_address = dl->NS;
            state = PASS_TOKEN;// Tusage_timeout
            if(DBG) printf("%02X:Pass Token %02X\n", dl->TS, dl->NS);
            bacnet_mstp_header(dl, 0x00/* Token Pass */, dl->NS, false);
            osServiceTimerStart(svc, Tusage_timeout);
        } else {// продолжить сканирование
            dl->PS = PS;
            dl->destination_address = dl->PS;
            state = POLL_FOR_MASTER;// Tusage_timeout);
            if(DBG) printf("%02X:Poll For Master %02X\n", dl->TS, dl->PS);
            bacnet_mstp_header(dl, 0x01/* Poll For Master */, dl->PS, true);// \todo готовит пакет для отсылки,
            osServiceTimerStart(svc, Tusage_timeout);
        }
    } else 
	if (signals & DL_UNITDATA_confirm) { // 1)
//        printf("%02X:PFM Confirm\n", dl->TS);
		signals &= ~DL_UNITDATA_confirm;
		osServiceTimerStart(svc, Tusage_timeout);
	}
	if (signals & DL_UNITDATA_indication) {// 2 пришел ответ
        DataUnit_t transfer = {.buffer = dl->buffer, .size = dl->data_len};//BACNET_NPDU_LEN_MAX};
         DataUnit_t* tr = &transfer;

//        DataUnit_t transfer = {.buffer = dl->buffer, .size = BACNET_NPDU_LEN_MAX};
/// TODO двойная буферизация, возвращается не тот буфер, который передается функции, а предыдущий
//! 	DataUnit_t* tr = usart_recv(dl->handle, dl->buffer, BACNET_NPDU_LEN_MAX);
		int res;
		if (tr==NULL){
			res = osErrorResource;
		} else res = bacnet_mstp_indication (dl, tr->buffer, tr->size);
		if (res<0) {
			// битый пакет
			state = MSTP_IDLE;
			osServiceTimerStart(svc, Tno_token);
		} else
		//найден, заняться делом или продолжить сканирование, если нет дела
		if ((dl->destination_address == dl->TS) && (dl->FrameType == 0x02)) {// Reply To Poll For Master, ReceivedReplyToPFM
			dl->NS = dl->source_address;
			dl->PS=dl->TS;
			dl->TokenCount=0;
			dl->destination_address = dl->NS;
			state = PASS_TOKEN;
			if(DBG) printf("%02X:Token Pass %02X\n", dl->TS, dl->NS);
			bacnet_mstp_header(dl, 0x00/* Token Pass */, dl->NS, false);
			osServiceTimerStart(svc, Tusage_timeout);
			signals &= ~DL_UNITDATA_indication;// убрать!
			//osServiceTimerStart(self, Tusage_timeout);
		} else {// пришло что-то странное уступить роль мастера
			if(DBG) printf("%02X:Lost poll for master (Dest=%02X)\n",dl->TS, dl->destination_address);
			state = MSTP_IDLE;
			continue;
		}
	}
} break;
case USE_TOKEN: {// сюда попадаем только по continue
    DataUnit_MSTP_t* tr;
    if((tr=osAsyncQueueGet(&dl->datalink.queue))!=NULL) {// это единственное место где мы забираем из очереди.
        dl->FrameCount++;
/*! 9.7.2 Routing of BACnet Messages to MS/TP
When a BACnet network entity issues a DL-UNITDATA.request to a directly connected MS/TP data link, it shall set the
'data_expecting_reply' parameter of the DL-UNITDATA.request equal to the value of the 'data_expecting_reply' parameter of
the network protocol control information of the NPDU, which is transferred in the 'data' parameter of the request. */
        bool data_expecting_reply = (tr->buffer[1] & 0x04)!=0;//tr->status & 1;//  см NPDU.control & 0x04 buffer[1]
        dl->destination_address = tr->device_id;//tr->device_id;// может быть LOCAL_BROADCAT
        uint8_t FrameType = data_expecting_reply? 0x05:0x06;
        if(DBG) printf("%02X:Transfer mstp use_token len=%d\n", dl->TS, tr->size);

        osServiceTimerStop(svc);

        int result = bacnet_mstp_request(dl, FrameType, tr->device_id, tr->buffer, tr->size);
        // ждем ответа на буфере отсылки, или ждем завершения отсылки
        if (result<0) {// ошибка ресурса, возникла в процессе отсылки
            tr->status = result;
            //g_cond_signal(&tr->condition);
            state = DONE_WITH_TOKEN;
            continue;
        } else
        if (data_expecting_reply) {
            tr->status = 0;
            //g_cond_signal(&tr->condition);
            //dl->request = tr;
			if (DBG) printf("%02X:Wait for Replay\n", dl->TS);
            osServiceTimerStart(svc, T_reply_timeout);// ждем ответа на буфере отсылки
            state = WAIT_FOR_REPLY;
        } else {
            tr->status = 0;// может быть инвалидным
            //g_cond_signal(&tr->condition);
            if ((dl->FrameCount < Nmax_info_frames)) {// есть что слать
                state = (osAsyncQueueEmpty(&dl->datalink.queue))? DONE_WITH_TOKEN: USE_TOKEN;
            } else { // dl->FrameCount >= Nmax_info_frames
                state = DONE_WITH_TOKEN;
            }
            continue;
        }
    }
    else {// NothingToSend
        //printf("%02X:DONE_WITH_TOKEN\n", dl->TS);
        state = DONE_WITH_TOKEN;
        continue;
    }
} break;
case WAIT_FOR_REPLY: {
    if ((event->status & (osEventSignal|osEventTimeout))==osEventTimeout){// T_reply_timeout
		osServiceTimerStop(svc);
		if (DBG) printf("%02X:Wait for Replay -- Timeout\r\n", dl->TS);
//        tr->status = osErrorTimeout;
//        g_cond_signal(&tr->condition);
        state = DONE_WITH_TOKEN;
        continue;
    }
    osServiceTimerStop(svc);
    if (event->status & osEventSignal) {
//		if(1) printf("%02X:Wait for Replay -- Signal\r\n", dl->TS);
        if (signals & DL_REPORT_indication) {
//            tr->status = osErrorResource;
//            g_cond_signal(&tr->condition);
            state = DONE_WITH_TOKEN;
            continue;

        } else
        if (signals & DL_UNITDATA_confirm) {// подтверждение отсылки, отсылка завершена
			if(DBG) printf("%02X:Wait for Replay -- Confirm\r\n", dl->TS);
            //N_RELEASE_req(dl);// отпустили буфер, данные отосланы
            signals &= ~DL_UNITDATA_confirm;
            if (dl->data_expecting_reply) {
                osServiceTimerStart(svc, T_reply_timeout);// ждем ответа на буфере отсылки
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
            //DataUnit_t* tr = usart_recv(dl->handle, dl->buffer, BACNET_NPDU_LEN_MAX);
            if(DBG) 
				printf("ReceivedValidFrame2\r\n");
            //int res = bacnet_mstp_indication(dl, tr->buffer, tr->size);// разбор пакета
//            tr->status = osErrorParameter;// по умолчанию
/*            if (res<0) {
                // битый пакет
                tr->status = osErrorParameter;
                state = MSTP_IDLE;
                osServiceTimerStart(svc, Tno_token);
            } else */
            if (dl->destination_address == dl->TS) {// адресован нам
                switch (dl->FrameType) {
                case  6:// BACnet Data Not Expecting Reply,
                case 33:{// BACnet Extended Data Not Expecting Reply
                    source_addr.len=1, source_addr.vmac_addr = dl->source_address;
                    source_addr.network_number = dl->datalink.network_number;
                    //tr->buffer+=8; tr->size = res;
                    if(0) printf("bacnet_datalink_indication\r\n");
                    bacnet_datalink_indication(&dl->datalink, &source_addr, dl->buffer+8, dl->data_len/*, false*/);
                    //tr->buffer = dl->buffer+8;
                    //tr->status = 0;
//                    g_cond_signal(&tr->condition);// надо записать ответ на этот буфер
                    state = DONE_WITH_TOKEN;
					continue;
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
                if(DBG) printf("ReceivedUnexpectedFrame\n");
//                tr->status = osErrorParameter;
//                g_cond_signal(&tr->condition);
                state = MSTP_IDLE;// + DL_UNITDATA_indication
                continue;
            }
//            g_cond_signal(&tr->condition);
        }
    }
} break;

case MSTP_IDLE: {
    if ((event->status & (osEventTimeout|osEventSignal))==osEventTimeout) {// LostToken
        if(DBG) printf("%02X: NO_TOKEN Tout=%d\n", dl->TS, (Tslot*dl->TS));
        dl->PS = dl->TS;
        state = NO_TOKEN;
        osServiceTimerStart(svc,/* Tno_token+*/(Tslot*dl->TS));// добавить довесок для устойчивости?
    } else
    if (signals & DL_REPORT_indication){// ReceivedInvalidFrame
        if(DBG) printf("%02X:DL_REPORT_indication -- invalid frame\n", dl->TS);
        // перезарядить
    } else
    if (signals & DL_UNITDATA_confirm){// ReceivedInvalidFrame
        if(DBG) printf("%02X:DL_UNITDATA_confirm\n", dl->TS);
		
        // перезарядить
    } else
    if (signals & DL_UNITDATA_indication) {// пришел запрос
        signals &= ~DL_UNITDATA_indication;
        
        DataUnit_t transfer = {.buffer = dl->buffer, .size = dl->data_len};//BACNET_NPDU_LEN_MAX};
         DataUnit_t* tr = &transfer;


//        DataUnit_t transfer = {.buffer = dl->buffer, .size = BACNET_NPDU_LEN_MAX};
//!        DataUnit_t* tr = usart_recv(dl->handle, dl->buffer, BACNET_NPDU_LEN_MAX);// возвращает заполненную структуру
        int res = bacnet_mstp_indication (dl, tr->buffer, tr->size);
        if (res<0) {
			printf("%02X: IDLE -- bad pkt %p, %d\n", dl->TS,tr->buffer, tr->size );
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
					if(DBG) printf("%02X:Reply to Poll For Master %02X\n", dl->TS, dl->source_address);
					bacnet_mstp_header(dl, 0x02, dl->source_address, false);// Reply To Poll for Master
				} else {// очищаем и ждем ответа
				}
				break;
			case 0x03:// Test_Request
				// надо проверить целостность вернуть все что пришло запросом
				if (dl->destination_address == dl->TS) {
					dl->destination_address = dl->source_address;
					bacnet_mstp_header(dl, 0x04/* Test_Response */,dl->source_address, false);// data_len?
				} else {// очищаем все в таблице, ждем отклика
				}
				break;
			case 32:// BACnet Extended Data Expecting Reply
			case 0x05:// BACnet Data Expecting Reply
				if (dl->destination_address == dl->TS) {// ReceivedDataNeedingReply
                    //tr->buffer+=8, tr->size = res;
					bacnet_datalink_indication(&dl->datalink, &source_addr, tr->buffer+8, res/*, true*/);
					state = ANSWER_DATA_REQUEST;
					osServiceTimerStart(svc, Treply_delay);
				} else {// BroadcastDataNeedingReply наш пакет, но отвечать не надо
                    //tr->buffer+=8, tr->size = res;
					bacnet_datalink_indication(&dl->datalink, &source_addr, tr->buffer+8, res/*, false*/);
				}
				break;
			case 33:// BACnet Extended Data Not Expecting Reply
			case 0x06:// BACnet Data Not Expecting Reply
				if ((dl->destination_address == dl->TS) || (dl->destination_address == BROADCAST)) {// ReceivedDataNoReply
                    //tr->buffer+=8, tr->size = res;
					bacnet_datalink_indication(&dl->datalink, &source_addr, tr->buffer+8, res/* , false */);
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
    if (state == MSTP_IDLE) osServiceTimerStart(svc, Tno_token);
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
    if(DBG)  printf("%02X:Initialize\n", dl->TS);
    osServiceTimerStart(svc, Tno_token);// как сделать чтобы все включились и не подрались
} break; // должны прямиком попасть в IDLE
case ANSWER_DATA_REQUEST:{	/*  9.5.6.9 ANSWER_DATA_REQUEST, Treply_delay
 The ANSWER_DATA_REQUEST state is entered when a BACnet Data Expecting Reply, BACnet Extended Data Expecting
 Reply, a Test_Request, or a FrameType known to this node that expects a reply is received */
    if (event->status & osEventTimeout) {
        state = MSTP_IDLE;
        /// зарядить на прием
    } else
    if (signals & DL_RELEASE_request){// DeferredReply
        // then an immediate reply is not possible. Any reply shall wait until this node receives the token.
        dl->destination_address = dl->source_address; // -- заранее
        state = MSTP_IDLE;
        bacnet_mstp_header(dl, 0x07/* Reply Postponed */,dl->source_address, false);
    } else
    if (signals & DL_UNITDATA_request) {// Reply
        //DataUnit_t* tr = dl->transfer;
        DataUnit_t* tr=dl->response;//osAsyncQueueGet(&dl->queue);/// TODO это может быть другая очередь, я жду определенный запрос, на этом очередь блокируется

        dl->destination_address = dl->source_address; // -- заранее
        state = MSTP_IDLE;
        bacnet_mstp_request(dl, 0x06/* Reply */, dl->source_address, tr->buffer, tr->size);// \todo готовит пакет для отсылки,
    }
} break;

}
break;
}
    dl->state = state;
if(0) printf("%02X:signals %02X/%02X %02X %s\n", dl->TS, state, prev, signals, (signals& DL_UNITDATA_confirm)?"confirm":"other");
#ifdef _WIN32
    if (signals!=0) {
        printf("%02X:signals %02X/%02X %s\n", dl->TS, state, prev, (signals& DL_UNITDATA_confirm)?"confirm":"other");
        _Exit(1);
    }
#endif // __WIN32
    return 0;
}

#if 0


DataLink_t *bacnet_mstp_datalink=NULL; // это временно, потом надо убрать


extern int service_run;
//extern void* osServiceInit(uint32_t id);
extern void  osServiceWorkFlow(void* ctx);
//extern void  osServiceRunAs(osService_t *svc, void* ctx);


#define BACNET_MSTP_THREAD_NAME "BACnet MS/TP"
static void* bacnet_mstp_master_thread(void* data)
{
    struct _mstp_ctx* dl = (void*)data;
    void* svc_ctx = osServiceInit(0);
    osService_t * svc = osServiceCreate(NULL, bacnet_mstp_master_service, data);
    osServiceTimerStart(svc, Tno_token);
    osServiceRunAs(svc, svc_ctx);
printf("Main BACnet MS/TP service run\n");
    struct timespec timeout = {.tv_nsec=1000000};
    int resp_len = 0;
    while (service_run) {
        osServiceWorkFlow(svc_ctx);
//        sched_yield();
#if defined(__linux__)
        extern int rs485_pdu_recv(void*, uint8_t *buffer, size_t max_len);
        int resp_len = rs485_pdu_recv(dl->handle, dl->buffer, 2048);
#endif // defined
        if (resp_len>0) {
            uint8_t *buf = dl->buffer;
            int i;
            for(i=0;i<resp_len;i++) {
                printf (" %02X", buf[i]);
            }
            printf ("\n");
            uint16_t data_len;// = htons(*(uint16_t*)buf[5]);
            if (buf[0]==0x55 && buf[1]==0xFF) {
                if (bacnet_mstp_hdr_check(dl, buf, &data_len)) {
                    if (data_len>0 && data_len+10 == resp_len) {
                        uint16_t crc16 = *(uint16_t *)&buf[data_len+8];
                        if(crc16 == bacnet_crc16(buf+8, data_len)){
                            dl->data_len = data_len;
                            osServiceSignal(svc, DL_UNITDATA_indication);
                        }
                    } else if (data_len==0 && data_len+8 == resp_len) {
                        dl->data_len = data_len;
                        osServiceSignal(svc, DL_UNITDATA_indication);
                    } else {// битый пакет
                    }
                }
            }
        }
        timeout.tv_sec=0;
        timeout.tv_nsec=1000000;
        clock_nanosleep(CLOCK_MONOTONIC, 0, &timeout,NULL);
    }
    return NULL;
}
#include "r3_driver.h"
extern void* rs485_pdu_recv_th(void* vfd);
__attribute__((constructor)) static void _init()
{
    struct _mstp_ctx * ctx = malloc(sizeof(struct _mstp_ctx));
    ctx->buffer = malloc(2048);
//    ctx->request = NULL;
    ctx->response = NULL;
    ctx->TS = 0;// 0x1F;
    ctx->state = MSTP_INITIALIZE;
    bacnet_datalink_init(&ctx->datalink, LOCAL_NETWORK);
//    R3BusDriver* bus=NULL;
//    ctx->bus = &r3_rs485_driver;
    ctx->handle = rs485_init("/dev/ttyO2", "baudrate=38400 parity=None rts_gpio=49");
#if 0//defined(__linux__)
    g_thread_new("RS485 pdu recv thread",rs485_pdu_recv_th, ctx->handle);
#endif // defined
//    printf("Create BACnet MS/TP service\n");
    g_thread_new(BACNET_MSTP_THREAD_NAME, bacnet_mstp_master_thread, ctx);

    bacnet_mstp_datalink = &ctx->datalink;
}
#endif
/*! \brief Анализ шапки пакета, 
	если шапка с ошибкой, то пропускаем только шапку
	Если пакет адресован не нам, то пропускаем данные от пакета учитывая длину.
 */
static int bacnet_mstp_header_check(struct _mstp_ctx *dl, const uint8_t * buffer, uint16_t *data_len)
{

    //hdr->FrameType		= buffer[0];
	dl->destination_address = buffer[1];
	dl->source_address		= buffer[2];
//    *data_len = ntohs(*(uint16_t*)&buffer[3]);
    uint8_t hdr_crc = buffer[5];
	return (bacnet_crc8(buffer)==hdr_crc && dl->source_address!=0xFF);// нарушена целостность пакета
}

#define BACNET_LEN_MAX 256
#define MSTP_FLAGS (1)
#define MSTP_UNITDATA_indication (1<<MSTP_FLAGS)
#define MSTP_UNITDATA_confirm    (2<<MSTP_FLAGS)
#define MSTP_REPORT_indication   (4<<MSTP_FLAGS)
#define MSTP_PORT 2
enum _DL_State {_IDLE, _ANSWER_DATA_REQUEST};

uint32_t clientID_mem CONFIG_ATTR = 0x01;// адрес по умолчанию для автоматической настройки

static int mstp_UNITDATA_request(struct _DataLink *dl, const BACnetAddr_t* DA, uint8_t* data, size_t data_len)
{
	DataUnit_MSTP_t* tr = g_slice_new(DataUnit_MSTP_t);
    tr->buffer = data;
    tr->size   = data_len;
	tr->status = 0;
	tr->device_id = DA->mac[0];
	printf("BACnet MSTP_UNITDATA_request Len = %p %p, %d\r\n", tr, data, data_len);
	
	osAsyncQueuePut(&dl->queue, tr);// поставить в очередь
	return 0;
}

void bacnet_mstp_thr(void const* user_data)
{
	DeviceInfo_t * local_device = (void*)user_data;
	/* запустить службу обработки протокола */
    struct _mstp_ctx* ctx = malloc(sizeof(struct _mstp_ctx));//(void*)user_data;
    uint8_t * buffer = ctx->buffer = malloc(2048);
//    ctx->request = NULL;
    ctx->response = NULL;
    ctx->TS = clientID_mem;// 0x1F;
    ctx->state = MSTP_INITIALIZE;
//	Device_t* device = local_device->device;
    bacnet_datalink_init(&ctx->datalink, LOCAL_NETWORK, local_device);
	ctx->datalink.UNITDATA_request=mstp_UNITDATA_request;

	NetworkPort_t* port = malloc(sizeof(NetworkPort_t));
	port->network_type = MSTP;
	port->routing_table.list=NULL;
	port->datalink= &ctx->datalink;

	port->next=local_device->device->network_ports;
	local_device->device->network_ports = port;



//    void* svc_ctx = osServiceInit(0);
//printf("Main BACnet MS/TP service run\r\n");
	/* запустить службу обработки протокола */
	uint8_t clientID = clientID_mem;
	osEvent event;
	//struct _DL_hdr hdr = {.state=_IDLE, .TS=clientID};
	//dl->owner = osThreadGetId();
	ctx->handle = usart_open(MSTP_PORT, osSignalRef(osThreadGetId(), MSTP_FLAGS));
	printf("BACnet MS/TP addr=%02X\r\n", clientID);

    osService_t * svc = osServiceCreate(NULL, bacnet_mstp_master_service, (void*)ctx);
    osServiceTimerStart(svc, Tno_token);
    osServiceRun(svc);

	do {
		osTransfer * transfer = usart_recv(ctx->handle, buffer, BACNET_LEN_MAX);
		uint32_t signal_mask, timeout=Tno_token;
		
		signal_mask = MSTP_UNITDATA_indication|MSTP_UNITDATA_confirm|MSTP_REPORT_indication;//1<<1;// используем три флага DL_indication|DL_confirm| DL_report, 
		event = osSignalWait(signal_mask/*1<<1*/, timeout);// используем три флага indication|confirm|report, 
		if (event.status & osEventTimeout){
			//continue;
		} 
		if (event.status & osEventSignal) {
			uint32_t signals = event.value.signals;
			if (signals & MSTP_UNITDATA_confirm){// подтверждение отсылки ACK
				osSignalClear(osThreadGetId(),MSTP_UNITDATA_confirm);// 1<<1);
				osServiceSignal(svc, DL_UNITDATA_confirm);
				//printf("DL_UNITDATA_confirm\r\n");
			}
			if (signals & MSTP_REPORT_indication) {
				osSignalClear(osThreadGetId(),MSTP_REPORT_indication);// 1<<1);
				/* \todo проверить статус завершения трансфера */
				printf("Rx error = %d\r\n", transfer->status);
				// выполнить процедуру восстановления
			} else 
			if (signals & MSTP_UNITDATA_indication) {
				//printf("MSTP_UNITDATA_indication\r\n");
				osSignalClear(osThreadGetId(), MSTP_UNITDATA_indication);// 1<<1);
				int len = transfer->size;
				if (len >= 8 && buffer[0]==0x55 && buffer[1] ==0xFF) {// пропустили приамблу
					uint16_t data_len;
					if (bacnet_mstp_hdr_check(ctx, buffer, &data_len)) {
						if (data_len>0 && data_len+10 == len) {
							uint16_t crc16 = *(uint16_t *)&buffer[data_len+8];
							if(crc16 == bacnet_crc16(buffer+8, data_len)){
								ctx->data_len = len;
if (0){
	//printf("DL_UNITDATA_indication\r\n");
	int i;
	for(i=0;i<len;i++){
		printf(" %02X", buffer[i]);
	}
	printf("\r\n");
}
								osServiceSignal(svc, DL_UNITDATA_indication);
							}
						} else if (data_len==0 && data_len+8 == len) {
							ctx->data_len = len;
if (1){
	printf("DL_UNITDATA_indication\r\n");
	int i;
	for(i=0;i<len;i++){
		printf(" %02X", buffer[i]);
	}
	printf("\r\n");
}
							osServiceSignal(svc, DL_UNITDATA_indication);
//							printf("DL_UNITDATA_indication\r\n");
						} else {// битый пакет
						}
					}
				} else {// ошибка формата, запустить процедуру восстановления соедиенния
					printf("BACnet error Len = %d\r\n", len);
					if (len>8) len = 8;
					int i;
					for(i=0;i<len;i++){
						printf(" %02X", buffer[i]);
					}
					printf("\r\n");
					continue;

				}
			}
		}
	} while(1);

}
