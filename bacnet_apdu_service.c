#include "cmsis_os.h"
#include <stdio.h>

#include "atomic.h"
#include "r3_slist.h"

#include "bacnet_net.h"
#include "bacnet_error.h"
#include "bacnet_services.h"


enum _BACnetAbortReason /* ENUMERATED */{
//OTHER 	=(0),
BUFFER_OVERFLOW 	=(1),
INVALID_APDU_IN_THIS_STATE 	=(2),
PREEMPTED_BY_HIGHER_PRIORITY_TASK 	=(3),
SEGMENTATION_NOT_SUPPORTED 	=(4),
_SECURITY_ERROR 	=(5),
INSUFFICIENT_SECURITY 	=(6),
WINDOW_SIZE_OUT_OF_RANGE 	=(7),
APPLICATION_EXCEEDED_REPLY_TIME 	=(8),
OUT_OF_RESOURCES 	=(9),
TSM_TIMEOUT 	=(10),
APDU_TOO_LONG 	=(11),
// ...
};

int bacnet_apdu_requesting_service (osService_t *svc, void* service_data);


/*! 6.1 Network Layer Service Specification
Conceptually, the BACnet network layer provides an unacknowledged connectionless form of data unit transfer service to the
application layer. The primitives associated with the interaction are the N-UNITDATA.request, the N-UNITDATA.indication,
the N-RELEASE.request, and the N-REPORT.indication. These primitives provide parameters as follows:

N-UNITDATA.request (
    destination_address,
    data,
    network_priority,
    data_expecting_reply,
    security_parameters
)
N-UNITDATA.indication (
	source_address,
	destination_address,
	data,
	network_priority,
	data_expecting_reply,
	security_parameters
)
N-RELEASE.request (
destination_address
)
N-REPORT.indication (
peer_address,
error_condition,
error_parameters,
security_parameters
)

 */
/*! \brief запрос к сетевому уровню на отсылку прикладных данных по сети
    \param hdr заголовок APDU
    \param hdr_len длина заголовока APDU не превышает 8 байт.Используется вырваниваение по правому краю
	\param data прикладные данные APDU
	\param data_len размер данных в байтах

Мы выделяем две части сообщения, data - содержит прикладные данные для отсылки, шапка копируется перед данными, 
в буфере data должно быть зарезервировано место для копирования заголовка сообщения
*/
static void N_UNITDATA_req(BACnetNPCI* npci, uint8_t *pdu, size_t pdu_len, uint8_t * data, size_t data_len)
{

    // дописывать шамки - это свойство datalink_indication
    if (data==NULL){
        data = pdu;
    } else {
        if (pdu_len) {
            data -= pdu_len, data_len += pdu_len;
            __builtin_memcpy(data, pdu, pdu_len);
            // *(uint32_t*)(data-8) = *(uint32_t*)(pdu+0);
            // *(uint32_t*)(data-4) = *(uint32_t*)(pdu+4); -- этот вариант нравится больше
        }
    }
#if 0
    if (npci->DA.len!=6 && npci->DA.len!=0)
    {// если не напрямую к сети подключено, используем роутер по умолчанию
        npci->control |= 0x20; // 5:Destination
        size_t pdu_len = 6 + npci->DA.len;
        data -= pdu_len, data_len += pdu_len;

        uint8_t * npdu = data;
        npdu[0] = 0x01;
        npdu[1] = npci->control;
        npdu[2] = npci->DA.network_number>>8;
        npdu[3] = npci->DA.network_number;
        npdu[4] = npci->DA.len;
        if (npci->DA.len==1) {
            npdu[5] = npci->DA.mac[0];
        }
        npdu[5+npci->DA.len] = 0x0F;// hop_count
    } else {
        size_t pdu_len = 2;
        data -= pdu_len, data_len += pdu_len;
        uint8_t * npdu = data;
        npdu[0] = 0x01;
        npdu[1] = npci->control;
    }
#endif // 0
    bacnet_relay(/*npci->device,*/ npci, data, data_len);
/*
    DataLink_t* dl = bacnet_ipv4_datalink;// ? где бы взять?
    //DL->UNITDATA.request(npci, data, data_len);
// все остальное -- это метод класса datalink
    DataUnit_t* tr = g_slice_new(DataUnit_t);//osMailAlloc(service->queue_id, millisec); // выделить память
//    cmd->condition = &cond;
    g_cond_init(&tr->condition);
    tr->status = npci->control;// security_parameters|data_expecting_reply|network_priority
    tr->buffer = data;
    tr->size   = data_len;
//    tr->device_id = device_id;

    g_mutex_lock(&dl->mutex);
    osAsyncQueuePut(&dl->queue, tr);// поставить в очередь
// отсылку хотим сделать проще, без таких извратов как condition и mutex

    guint64 timestamp = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
    if (g_cond_wait_until (&tr->condition, &dl->mutex, timestamp))
    {// произошло событие
        g_print("Cond recv'd (%d) len=%d\n", tr->status, tr->size);
        g_cond_clear(&tr->condition); //osCondDelete(cond_id);

    } else {
	// надо умудриться выкинуть из очереди команду
    }
    g_slice_free(DataUnit_t, tr);

    g_mutex_unlock(&dl->mutex); */
}

// флаги
#define SRV 0x01
#define NAK 0x02
//#define SA  0x02
#define MOR 0x04
#define SEG 0x08    // segmented
#define SEQ_MASK 0xFF
#define SEQ_BITS 8

#define ms (1000)
static const uint32_t RequestTimeout=256*ms;
static const uint32_t Number_Of_APDU_Retries=3;

enum _APDU_state {
	IDLE, SEGMENTED_REQUEST, SEGMENTED_RESPONSE, SEGMENTED_CONF, AWAIT_RESPONSE, AWAIT_CONFIRMATION
};

typedef struct _TSM TSM_t;
struct _TSM {
    uint8_t invoke_id;
	enum _APDU_state state:8;
	uint8_t service_choice;
	//BACnetAddr_t *peer;
	BACnetNPCI*npci;
	osService_t * svc;
	uint8_t *apdu;
	size_t apdu_len;// см total_length

	uint32_t timer; // штамп времени
	volatile  int32_t  ref_count;// счетчик пользователей объекта
//	volatile uint32_t* ref_signal;// ссылка на прикладной процесс который ждет подтверждение

	size_t seq_pos;// последовательная позиция чтения из буфера APDU
	size_t total_length;// полная длина для фрагментированных пакетов
	uint16_t MTU;
	uint8_t seq_number_prev;// InitialSequenceNumber
	uint8_t seq_number_last;// LastSequenceNumber
	uint8_t window_size;	// ActualWindowSize
	uint8_t DuplicateCount;	// ActualWindowSize
	uint8_t RetryCount;
	uint8_t SegmentRetryCount;
//	bool SentAllSegments;
};
static uint32_t invoke_id_assign();
static void invoke_id_free(uint8_t invoke_id);

static inline int32_t _unref(volatile int32_t * ptr)
{
    return __atomic_fetch_sub(ptr, 1, __ATOMIC_SEQ_CST);
}
/*! \brief объект используется в нескольких процессах, убиваться должен в обоих. */
static int tsm_unref(TSM_t* tsm)
{
    int32_t count;
    if((count = _unref(&tsm->ref_count))==0){// убивает кто-то один
        printf("Unref TSM ok\n");
        invoke_id_free(tsm->invoke_id);// очистим флаг
        g_slice_free(TSM_t, tsm);
	} else {
		printf("Unref TSM=%d\n", count);
    }
	return count;
}
/*! \brief запрос на отсылку прикладных данных с подтверждением
	\param npci			контекст соединения(Network Protocol Control Info), включает привязку к сетевому устройству и буферы обмена
	\param service_choice 	тип запроса, определяет формат кодирования данных
	\param apdu 		пакет прикладных данных
	\param apdu_len 	длина данных в байтах
	\return SUCCESS(0) или код ошибки
 */
void UNCONF_SERV_req(BACnetNPCI* npci, uint8_t service_choice, uint8_t *apdu, size_t apdu_len)
{
    uint8_t npdu[4];
    npdu[0] = BACnet_Unconfirmed_Request_PDU;
    npdu[1] = service_choice;
    npci->control &= ~NPCI_DATA_EXPECTING_REPLY;
    N_UNITDATA_req(npci, npdu, 2, apdu, apdu_len);
}
static GSList* apdu_ttl_list=NULL;

ttl_datalist_t device_active_sessions={NULL};
static void SendConfirmedUnsegmented(BACnetNPCI* npci, TSM_t* tsm)
{
    uint8_t pdu[4];
    pdu[0] = BACnet_Confirmed_Request_PDU;// SA?
    pdu[1] = 0x05;
    pdu[2] = tsm->invoke_id;
    pdu[3] = tsm->service_choice;
    npci->control |= NPCI_DATA_EXPECTING_REPLY;
    N_UNITDATA_req(npci, pdu, 4, tsm->apdu, tsm->apdu_len);
}

/*! \brief запрос на отсылку прикладных данных с подтверждением
	\param npci			контекст соединения(Network Protocol Control Info), включает привязку к сетевому устройству и буферы обмена
	\param service_choice 	тип запроса, определяет формат кодирования данных
	\param apdu 		пакет прикладных данных
	\param apdu_len 	длина данных в байтах
	\return SUCCESS(0) или код ошибки
 */
int   CONF_SERV_req(BACnetNPCI* npci, uint8_t service_choice, uint8_t *apdu, size_t apdu_len)
{
    uint8_t invoke_id = invoke_id_assign();
    TSM_t *tsm = g_slice_new(TSM_t);
    tsm->invoke_id = invoke_id;
    tsm->npci = npci;/// TODO надо сохранять только адрес назначения DA
    tsm->state= IDLE;
    tsm->apdu = apdu;
    tsm->apdu_len = apdu_len;
    tsm->svc = npci->svc;
    // запрос является частью сценария, который будет вызваться при получении отклика.
    tsm->service_choice = service_choice;
    tsm->timer = osKernelSysTick();// _TimerStart(RequestTimeout);
//    tsm->ref_signal = NULL;// ref_signal
    tsm->ref_count = 0;//2 процесса
    ttl_datalist_push(&device_active_sessions, invoke_id, tsm);
    if (1){// SendConfirmedUnsegmented
        printf ("SendConfirmedUnsegmented\r\n");
        tsm->RetryCount=0;
//        tsm->SentAllSegments=TRUE;
        tsm->state = AWAIT_CONFIRMATION;
        SendConfirmedUnsegmented(npci, tsm);
        g_slist_prepend_atomic(&apdu_ttl_list, tsm);
    }
    return 0;
}


BACnetService_t     device_unconfirmed_service[16] = {
[I_AM] = {I_Am_ind},
[WHO_IS] = {Who_Is_ind},
[TIME_SYNCHRONIZATION] = {TimeSynchronization_ind},
[UNCONFIRMED_TEXT_MESSAGE] = {ConfirmedTextMessage_ind},
[UNCONFIRMED_PRIVATE_TRANSFER] = {ConfirmedPrivateTransfer_ind},
[WHO_AM_I] = {Who_Am_I_ind},
[YOU_ARE] = {You_Are_ind},
};

BACnetConfService_t device_confirmed_service[32] = {
//[ATOMIC_READ_FILE] = {AtomicReadFile_ind},
[CONFIRMED_TEXT_MESSAGE] = {ConfirmedTextMessage_ind, ConfirmedTextMessage_cnf},
// -- File Access Services
[ATOMIC_READ_FILE]  = {},
[ATOMIC_WRITE_FILE] = {AtomicWriteFile_ind, AtomicWriteFile_cnf},
// -- Object Access Services
[ADD_LIST_ELEMENT]  = {AddListElement_ind},
[REMOVE_LIST_ELEMENT] = {RemoveListElement_ind},
[CREATE_OBJECT] = {CreateObject_ind},
[DELETE_OBJECT] = {DeleteObject_ind},
[READ_PROPERTY] = {ReadProperty_ind, ReadProperty_cnf},
[READ_PROPERTY_MULTIPLE] = {ReadPropertyMultiple_ind, ReadPropertyMultiple_cnf},
[WRITE_PROPERTY] = {WriteProperty_ind},
[WRITE_PROPERTY_MULTIPLE] = {WritePropertyMultiple_ind},
[READ_RANGE] = {ReadRange_ind},
// -- Remote Device Management Services
[DEVICE_COMMUNICATION_CONTROL] ={},
[CONFIRMED_PRIVATE_TRANSFER] ={ConfirmedPrivateTransfer_ind},
[REINITIALIZE_DEVICE] ={ReinitializeDevice_ind},

};
#define ASSERT(x) if (!(x)) return 0
int error_decode(BACnetNPCI* npci, uint8_t * buf, int size)
{
    ASSERT(*buf++ == 0x91);
    uint8_t error_class = *buf++;
    ASSERT(*buf++ == 0x91);
    uint8_t error_code = *buf++;
    npci->response.result = ERROR(error_class, error_code);
    return 4;
}
int bacnet_apdu_service(DataLink_t* dl, BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
	
/*! \see 5.4 Application Protocol State Machines
    When a PDU is received from the network layer, the PDU type, the source and destination BACnetAddresses, and the Invoke
ID (if any) of the PDU shall be examined to determine the type (requesting BACnet-user or responding BACnet-user) and the
identity of the TSM to which the PDU shall be passed. If no such TSM exists, one shall be created. */
    // Выбрать сервис, requesting или responding
    uint8_t pdu_type  = buffer[0];
    uint8_t invoke_id;// = buffer[1];
    switch (pdu_type) {
//    case (BACnet_Confirmed_Request_PDU|SA):
    case BACnet_Confirmed_Request_PDU: {
        uint8_t maxseg = buffer[1];
        invoke_id = buffer[2];
        uint8_t service_choice = buffer[3];
        int result;
        if (service_choice>31) {
            result = osErrorParameter;
        } else
        if (device_confirmed_service[service_choice].indication) {
            result = device_confirmed_service[service_choice].indication(npci, buffer+4, size -4);
        } else {
        }
        if (result==0) {// send_simpleAck
            uint8_t *pdu = buffer;
            pdu[1] = BACnet_SimpleACK_PDU;
//            pdu[2] = invoke_id;
//            pdu[3] = service_choice;
            npci->control = 0;
            npci->DA = npci->SA;
            N_UNITDATA_req(npci, buffer+1, 3, npci->response.buffer+16, 0);
        } else if (result>0) {
            uint8_t *pdu = buffer;
            pdu[1] = BACnet_ComplexACK_PDU;
//            pdu[2] = invoke_id;
//            pdu[3] = service_choice;
            npci->control =0;// &= ~NPCI_DATA_EXPECTING_REPLY;
            npci->DA = npci->SA;
            N_UNITDATA_req(npci, buffer+1, 3, npci->response.buffer+16, result);
        } else {// Error
            uint8_t *pdu = buffer;
            pdu[1] = BACnet_Error_PDU;
//            pdu[2] = invoke_id;
//            pdu[3] = service_choice;
            result = ~result;
            pdu[4] = 0x91;
            pdu[5] = result>>16;
            pdu[6] = 0x91;
            pdu[7] = result;
            //pdu[6] = result;
            npci->control =0;// &= ~NPCI_DATA_EXPECTING_REPLY;
            npci->DA = npci->SA;
            N_UNITDATA_req(npci, buffer+1, 7, npci->response.buffer+16, 0);
        }
    } break;
    case BACnet_Unconfirmed_Request_PDU: {
        uint8_t service_choice = buffer[1];
        printf("BACnet_Unconfirmed_Request[%d] indication\r\n", service_choice);
        if (service_choice>13) {
        } else
        if (device_unconfirmed_service[service_choice].indication) {
            device_unconfirmed_service[service_choice].indication(npci, buffer+2, size -2);
        }
        else {
            printf("\t-- not found\r\n");
        }
//        osServiceSignal(apdu_responding_service, N_UNITDATA_indication);
    } break;
    default:
    case BACnet_Abort_PDU:
		printf("APDU: BACnet_Abort_PDU\r\n");
		break;
	case BACnet_Reject_PDU: {
		printf("APDU: BACnet_Reject_PDU\r\n");
		invoke_id = buffer[1];
		uint8_t service_choice = buffer[2];
		TSM_t *tsm = ttl_datalist_get(&device_active_sessions, invoke_id);// атомарно!!!
		if (tsm!=NULL) {

            tsm->state = IDLE;
            if (tsm->svc) {
                osServiceSignal(tsm->svc, REJECT_indication);
            }
		}
	} break;
    case BACnet_SegmentACK_PDU:
		printf("APDU: SegmentACK\r\n");
		break;
        break;
    case BACnet_Error_PDU:
    case BACnet_SimpleACK_PDU:
    case BACnet_ComplexACK_PDU: {
        invoke_id = buffer[1];
        uint8_t service_choice = buffer[2];
		if (pdu_type == BACnet_Error_PDU) {
			printf("APDU:  Error_PDU (%d)\r\n", invoke_id);
		} else
			printf("APDU: %s (%d)\r\n", pdu_type==BACnet_SimpleACK_PDU? "SimpleACK":"ComplexAck", invoke_id);

        TSM_t *tsm = ttl_datalist_get(&device_active_sessions, invoke_id);// атомарно!!!
		
        if (tsm!=NULL && service_choice==tsm->service_choice) {
			int offset = 3;
			if (pdu_type == BACnet_Error_PDU) {
				offset += error_decode(tsm->npci, buffer+offset, size -offset);
			}
            //osServiceTimerStop(tsm->svc);
            if (device_confirmed_service[service_choice].confirm) {
				//printf("!!!ACK Confirmed Service!!!\r\n");
                device_confirmed_service[service_choice].confirm(tsm->npci, buffer+offset, size -offset);
            } else {
                printf("!!!ACK Not Confirmed Service!!!\n");
				int i;
				for(i=0; i< size; i++){
					printf(" %02X", buffer[i]);
				}
				printf("\r\n");
            }
            if (tsm->svc) {
                osServiceSignal(tsm->svc, CONF_SERV_confirm);
            } else {
                printf("APDU: No CONF_SERV_confirm service=%d (%d)!!!\r\n", service_choice, invoke_id);
            }

            //invoke_id_free(invoke_id);
//            tsm_unref(tsm);
            //g_slice_free(TSM_t, tsm);
            tsm->state = IDLE;
        } else {
			printf("!!!TSM Not found (%d)!!!\r\n", invoke_id);
		}
    } break;
    }
    return 0;
}
static inline void _TimerRestart(uint32_t* timer, uint32_t timeout)
{
    *timer+=osKernelSysTickMicroSec(timeout);
}
static inline int  _TimerExpired(uint32_t timer_ts, uint32_t timestamp, uint32_t timeout)
{
	return (((uint32_t)(timestamp - timer_ts))>=osKernelSysTickMicroSec(timeout));
}
#if 0
static void _queue_update(osAsyncQueue_t* queue)
{
    GSList * tail = (GSList *)atomic_pointer_exchange(&queue->tail, NULL);
    if (tail) {
        GSList* list = tail;
        while(list->next) list=list->next;
        list->next = (GSList*)queue->head; // tail->next = queue->head
        queue->head = tail;
    }
}
#endif // 0
static uint32_t application_indication(TSM_t * tsm, uint32_t signal)
{
    if (tsm->svc==NULL) return 0;
    return osServiceSignal(tsm->svc, signal);
}
// связка CONF_SERV -> RequestTimeout bacnet_apdu_ttl_service
int bacnet_apdu_ttl_service(osService_t *svc, void* user_data)
{
    uint32_t timestamp = osKernelSysTick();
    GSList* head = (GSList*)atomic_pointer_exchange(&apdu_ttl_list, NULL);// ссылка на элемент prev->next

    GSList**prev = &head;
    GSList* list;// = *prev;
    while ((list=*prev)!=NULL) {
        TSM_t * tsm = list->data;// атомарно
        //uint8_t state = tsm->state;

        if (tsm->state == AWAIT_CONFIRMATION) {
            if (_TimerExpired(tsm->timer, timestamp, RequestTimeout)){
                if (tsm->RetryCount >= Number_Of_APDU_Retries) { // FinalTimeout
                    printf("FinalTimeout (%d)\r\n", tsm->invoke_id);
                    tsm->apdu[0] = TSM_TIMEOUT;
                    application_indication(tsm, ABORT_indication);
                    tsm->state = IDLE;
                } else
                {// TimeoutUnsegmented
                    printf("TimeoutUnsegmented (%d)\r\n", tsm->invoke_id);
                    tsm->RetryCount++;
                    SendConfirmedUnsegmented(tsm->npci, tsm);
                    //state = AWAIT_CONFIRMATION;
                    _TimerRestart(&tsm->timer, RequestTimeout);//Start(svc, RequestTimeout);
    //                N_UNITDATA_req(npci, pdu, pdu_len, tsm->apdu, tsm->apdu_len);
                }
            }
        }
        //tsm->state = state;
        GSList *next = list->next;
        if (tsm->state == IDLE) {
            if (tsm_unref(tsm)==0) {
				printf("!!Drop tsm (%d)\r\n", tsm->invoke_id);
				//_Exit(21);
				/// исключить из списка
				*prev = next;
				g_slice_free(GSList, list);
			}
        } else {
            prev = &list->next;
        }
    }
    if (head!=NULL) *prev = (GSList*)atomic_pointer_exchange(&apdu_ttl_list, head);/// атомарно добавляем в конец очереди

    osServiceTimerRestart(svc);//
    return 0;
}
#if 0
int bacnet_apdu_ttl_service(osService_t *svc, void* user_data)
{
    TSM_t *tsm = user_data;
    BACnetNPCI *npci = tsm->npci;
    uint8_t state = tsm->state;
    void send_conf_unsegmented(){
        uint8_t pdu[4];
        pdu[0] = BACnet_Confirmed_Request_PDU;// SA?
        pdu[1] = 0x05;
        pdu[2] = tsm->invoke_id;
        pdu[3] = tsm->service_choice;
        npci->control |= NPCI_DATA_EXPECTING_REPLY;
        N_UNITDATA_req(npci, pdu, 4, tsm->apdu, tsm->apdu_len);
    }
    void application_indication(uint32_t signal){
//        tsm->signal_ref = signal;
    }
    osEvent* event = osServiceGetEvent(svc);
    if (event->status & osEventTimeout){
        if (state == AWAIT_CONFIRMATION) {
            if (tsm->RetryCount >= Number_Of_APDU_Retries) { // FinalTimeout
                printf("FinalTimeout\r\n");
                tsm->apdu[0] = TSM_TIMEOUT;
                application_indication(ABORT_indication);
                invoke_id_free(tsm->invoke_id);
                state = IDLE;
            } else
            {// TimeoutUnsegmented
                printf("TimeoutUnsegmented\r\n");
                tsm->RetryCount++;
                send_conf_unsegmented();
                state = AWAIT_CONFIRMATION;
                osServiceTimerStart(svc, RequestTimeout);
//                N_UNITDATA_req(npci, pdu, pdu_len, tsm->apdu, tsm->apdu_len);
            }
        }
    } else {

    }
    tsm->state = state;
    if (state == IDLE) {
        tsm_unref(tsm);
    }
    return (state==IDLE)?-1:0;
}
#endif // 0
#if 0
int bacnet_apdu_responding_service(osService_t *svc, void* user_data)
{
    {//  nested functions
    }// !nested functions
    osEvent* event = osServiceGetEvent(svc);
    if (event->status & osEventTimeout) {// If RequestTimer becomes greater than Tout,
        if (tsm->state == AWAIT_RESPONSE) {
            _send_abort_pdu(ABORT_APPLICATION_EXCEEDED_REPLY_TIME);
        } // просто таймер сработал
        tsm->state = IDLE;
        // выключить таймер
    } else
    if (event->status & osEventSignal) {
		uint32_t signals = event->value.signals;
        if (signals & CONF_SERV_response) {// пришел ответ от прикладного уровня
            DataUnit_t *tr = ctx->apdu_buffer;
            if (tr->status < 0){// ответ отрицательный надо ошибку оформить
                send_error(tr->status);
            } else
            if (tr->size > 0){
                send_complex_ack(tr);
            } else {
                send_simple_ack(tr);
            }
        } else
        if (signals & N_UNITDATA_indication) {// пришел запрос от сетевого уровня
            DataUnit_t *cmd = osAsyncQueueGet();
            bacnet_apdu_responding_fsm(cmd->buffer, cmd->len);
        } else
        if (signals & N_SEC_ERR_indication) {// SecurityError_Received
            osServiceTimerStop(svc);
            state = APDU_IDLE;
        } else
        if (signals & N_REPORT_indication) {// SendAbort
            send_abort();
        }
    }
    return 0;
}
#endif // 0
#if 0
int bacnet_apdu_requesting_service(osService_t *svc, void* user_data)
{
    {//  nested functions
    }// !nested functions
    osEvent* event = osServiceGetEvent(svc);
    if (event->status & osEventTimeout) {
        switch (tsm->state) {
        case SEGMENTED_REQUEST: {} break;
        case AWAIT_CONFIRMATION: {} break;
        }
    } else
    if (event->status & osEventSignal) {
   		uint32_t signals = event->value.signals;
   		if (signals & UNCONF_SERV_request) {
            send_unconf_request_pdu();
            state = IDLE;
   		} else
   		if (signals & CONF_SERV_request) {
            if (segmented_request()){
                if (tr->size > maximum_transmittable_length) {
                    tr->status = osErrorNoMemory;
                    // завершить трансфер
                } else {// SendConfirmedSegmented отослалть окно с фрагментами

                }
            } else {// SendConfirmedUnsegmented
            }
   		}

    }
}
#endif // 0

/*! \brief 5.4.3 Function FillWindow

The function "FillWindow" sends PDU segments either until the window is full or until the last segment of a message has
been sent. No more than T_seg may be allowed to elapse between the receipt of a SegmentACK APDU and the transmission of
a segment. No more than T_seg may be allowed to elapse between the transmission of successive segments of a sequence

	\param NPCI network protocol control information
*/
static bool FillWindow(BACnetNPCI *npci, TSM_t*tsm, uint8_t pdu_type, uint8_t seq_number, uint8_t *pdu, size_t length, size_t ActualWindowSize, uint16_t MTU)
{
    uint8_t npdu[8];
    tsm->npci->control |= NPCI_DATA_EXPECTING_REPLY;
    npdu[0] = pdu_type|SEG|MOR;
    npdu[1] = 0x05;
    npdu[2] = tsm->invoke_id;
    //npdu[3] = (seq_number++) & SEQ_MASK;
    npdu[4] = tsm->window_size;// 'window-size';
    npdu[5] = tsm->service_choice;
	while (MTU < length) {// последний сегмент
        npdu[3] = (seq_number++) & SEQ_MASK;
		N_UNITDATA_req (npci, npdu, 6, pdu, MTU);
//		N_UNITDATA_req (DA, pdu_type|MOR, (seq_number++) & SEQ_MASK , pdu, MTU, /* data_expecting_reply */NPCI_control);
		pdu += MTU, length-=MTU;
		if (--ActualWindowSize==0) return false; // не все сегменты ососланы
	}
	npdu[0] = pdu_type|SEG;
	npdu[3] = (seq_number++) & SEQ_MASK;
	N_UNITDATA_req (npci, npdu, 6, pdu, length);
//	N_UNITDATA_req (DA, pdu_type, (seq_number++) & SEQ_MASK, pdu, length, /* data_expecting_reply */NPCI_control);
	return true;
}
static volatile uint32_t allocation_map[8]={0};
static void invoke_id_free(uint8_t invoke_id)
{
    uint32_t mask = 1UL<<(invoke_id&0x1F);
    uint32_t idx  = (invoke_id>>5);
    atomic_fetch_and(&allocation_map[idx], ~mask);
}

/// todo теряется нолик
/// todo если свободных ресурсов нет,то надо возвращать -1
static uint32_t invoke_id_assign()
{
    static volatile uint32_t invoke_id=0;
#if 0
    return atomic_fetch_add(&invoke_id, 1) & 0xFF;
#else
//    return invoke_id = (invoke_id+1)&0xFF;
    uint32_t id, mask, idx;
    do{
        id = atomic_fetch_add(&invoke_id, 1) & 0xF;
        mask = 1UL<<(id&0x1F);
        idx  = (id>>5);
    } while ((atomic_fetch_or(&allocation_map[idx], mask) & mask)!=0);
    return id;
#endif // 0
}
/*! 5.4.4 State Machine for Requesting BACnet User (client)

*/
int bacnet_apdu_requesting_service (osService_t *svc, void* service_data)
{
    uint8_t pdu[6];
    size_t pdu_len = 0;
	TSM_t  *tsm = service_data;
    osEvent* event = osServiceGetEvent(svc);
	BACnetNPCI* npci = tsm->npci;
//	Device_t* device = ctx->device;
//	DataLink_t* dl = ctx->dl;
//	TSM_t * tsm = &ctx->tsm;
	int state = IDLE;//tsm->state; по умолчанию на выходе

/*! Nested functions
APDUs sent by requesting BACnet-users (clients):
    BACnet-Unconfirmed-Request-PDU
    BACnet-Confirmed-Request-PDU
    BACnet-SegmentACK-PDU with 'server' = FALSE
    BACnet-Abort-PDU with 'server' = FALSE
APDUs sent by responding BACnet-users (servers):
    BACnet-SimpleACK-PDU
    BACnet-ComplexACK-PDU
    BACnet-Error-PDU
    BACnet-Reject-PDU
    BACnet-SegmentACK-PDU with 'server' = TRUE
    BACnet-Abort-PDU with 'server' = TRUE
 \{
 */

	void send_abort(uint8_t reason) {// отвечаем в запросе
		//state=IDLE;
		pdu[0] = BACnet_Abort_PDU;// 'server'=FALSE
		pdu[1] = tsm->invoke_id;
		pdu[2] = reason;// abort reason WINDOW_SIZE_OUT_OF_RANGE
		npci->control &= ~NPCI_DATA_EXPECTING_REPLY;
		pdu_len=3;
	}
	void send_unconfirmed(uint8_t* apdu, int length) {/*! SendUnconfirmed
If UNCONF_SERV.request is received from the local application program,
then issue an N-UNITDATA.request with 'data_expecting_reply' = FALSE
to transmit a BACnet-UnconfirmedRequest-PDU, and enter the IDLE state.*/
		pdu[0] = BACnet_Unconfirmed_Request_PDU;
		pdu[1] = tsm->service_choice;
		pdu_len=2;
	}
	void send_segmentAck(uint8_t ActualWindowSize){
		pdu[0] = BACnet_SegmentACK_PDU;// 'server'=FALSE
		pdu[1] = tsm->invoke_id;
		pdu[2] = tsm->seq_number_last;
		pdu[3] = ActualWindowSize;
		npci->control &= ~NPCI_DATA_EXPECTING_REPLY;
		pdu_len=4;
	}
	void send_segmentNAK(uint8_t ActualWindowSize){
		pdu[0] = BACnet_SegmentACK_PDU|NAK;// 'server'=FALSE
		pdu[1] = tsm->invoke_id;
		pdu[2] = tsm->seq_number_last;
		pdu[3] = ActualWindowSize;
		pdu_len=4;
		npci->control &= ~NPCI_DATA_EXPECTING_REPLY;
	}
	void send_conf_request(){
		pdu[0] = BACnet_Confirmed_Request_PDU;// SA?
		pdu[1] = 0x05;
		pdu[2] = tsm->invoke_id;
		pdu[3] = tsm->service_choice;
		npci->control |= NPCI_DATA_EXPECTING_REPLY;
		pdu_len=4;
	}
	int  send_conf_seg_request(uint8_t *apdu, int length, size_t ProposedWindowSize){
		pdu[0] = BACnet_Confirmed_Request_PDU|SEG|MOR;// SA?
		pdu[1] = 0x05;
		pdu[2] = tsm->invoke_id;
		pdu[3] = tsm->seq_number_last;
		pdu[4] = ProposedWindowSize;
		pdu[5] = tsm->service_choice;
		npci->control |= NPCI_DATA_EXPECTING_REPLY;
		return 6;
	}
	void save_segment(uint8_t seq_number,uint8_t *buf, int length) {
		tsm->seq_number_last = seq_number;
		__builtin_memcpy(&tsm->apdu[tsm->seq_pos], buf, length);
		tsm->seq_pos += length;
	}
	void application_indication(uint32_t signal) {
	    if (device_confirmed_service[tsm->service_choice].indication!=NULL) {
            //device_confirmed_service[tsm->service_choice].indication(npci, )
//            atomic_fetch_or(device_confirmed_service[tsm->service_choice], signal);
//            if (tsm->service->process.status == osEventRunning)
	    }

	}
	void application_confirm(uint32_t signal) {
		if (tsm->svc) {// приложение..
			osServiceSignal(tsm->svc, CONF_SERV_confirm);
		}
	    //if (device_confirmed_service[tsm->service_choice].confirm!=NULL) 
		{
            //device_confirmed_service[tsm->service_choice].indication(npci, )
//            atomic_fetch_or(device_confirmed_service[tsm->service_choice], signal);
//            if (tsm->service->process.status == osEventRunning)
	    }

	}
/// \} !nested functions



	if (event->status & osEventTimeout) {
/*
		if (state == SEGMENTED_REQUEST) {
			if (tsm->SegmentRetryCount >= N_retry) {// FinalTimeout
				osServiceTimerStop(self);
				ABORT_ind(device, ABORT_TSM_TIMEOUT);
				state = APDU_IDLE;
			} else {// Timeout
				tsm->SegmentRetryCount++;
				FillWindow(ctx->device, &ctx->npci, &ctx->tsm, &pdu);/// \todo передать контекст
				state = SEGMENTED_REQUEST;
			}
		} else */
		if (tsm->state == AWAIT_CONFIRMATION) {
			if (tsm->RetryCount >= Number_Of_APDU_Retries) { // FinalTimeout
                printf("FinalTimeout\r\n");
                tsm->apdu[0] = TSM_TIMEOUT;
                application_indication(ABORT_indication);
			} else
/*			if (tr->size > maximum_transmittable_length) { // TimeoutSegmented
				osServiceTimerStop(self);
				tsm->RetryCount++;
				tsm->SegmentRetryCount=0, tsm->SentAllSegments=false,
				tsm->InitialSequenceNumber=0, tsm->ActualWindowSize=1;
				osServiceTimerStart(self, SegmentTimeout);
				pdu.type = (BACnet_Confirmed_Request_PDU|PDU_FLAG_SEG|PDU_FLAG_MOR);
				pdu.sequence_number=0;
				N_UNITDATA_req(device, &ctx->npci, &pdu, true);
				state = SEGMENTED_REQUEST;
			} else  */
			{// TimeoutUnsegmented
                printf("TimeoutUnsegmented\r\n");
				tsm->RetryCount++;
				send_conf_request();
                state = AWAIT_CONFIRMATION;
				osServiceTimerStart(svc, RequestTimeout);
//                N_UNITDATA_req(npci, pdu, pdu_len, tsm->apdu, tsm->apdu_len);
			}
		}
	} else
	if (event->status & osEventSignal) {
		uint32_t signals = event->value.signals;
		if (signals & UNCONF_SERV_request) {/*! SendUnconfirmed */
            printf ("SendUnconfirmed\n");
			// If UNCONF_SERV.request is received from the local application program,
			state = IDLE;// ничего не меняется
			send_unconfirmed(tsm->apdu, tsm->apdu_len);

		} else
		if (signals & CONF_SERV_request) {
			// from the local application program,
#if 0 // не поддерживаем сегменты
			DeviceInfo_t* device = ;
			DeviceInfo_t* remote = ;
			if (remote==NULL){
                printf ("CannotSend\n");
                ABORT_ind(device, OTHER, /* 'server' = */ FALSE);
			}
			else {// remote!=NULL
                uint32_t Max_Segments_Accepted = MIN(device->Max_Segments_Accepted,  remote->Max_Segments_Accepted);
                uint32_t Maximum_APDU_Length   = MIN(device->Maximum_APDU_Length,    remote->Maximum_APDU_Length);
                if (tr->size > Maximum_APDU_Length) {//  as determined according to Clause 5.2.1
                // Max_APDU_Length_Accepted property of the remote peer's Device object.
                    DeviceInfo_t * dev = bacnet_device_info(DA);
                    if (Max_Segments_Accepted ==0
                    || (Max_Segments_Accepted * Maximum_APDU_Length) < tr->size) {// CannotSend
                        printf ("CannotSend\n");
                        ABORT_ind(device, ABORT_APDU_TOO_LONG);// отвечаем прикладному сервису
                        state = IDLE;
                    } else
                    {// SendConfirmedSegmented
                        printf ("SendConfirmedSegmented\n");
                        pdu.type = BACnet_Confirmed_Request_PDU|SEG|MOR;
                        pdu.invoke_id = assign_invoke_id();
                        pdu.seq_number = 0;
                        pdu.window_size = 0;
                        tsm->InitialSequenceNumber = 0;
                        tsm->RetryCount=0;
                        tsm->SegmentRetryCount=0;
                        tsm->SentAllSegments=FALSE;
                        NPCI_control |= NPCI_DATA_EXPECTING_REPLY;
                        N_UNITDATA_req(tsm->DA, tsm->apdu, tsm->apdu_len, NPCI_control);
                        state = SEGMENTED_REQUEST;
                    }
                } else {}
			}
#endif
                {// SendConfirmedUnsegmented
                    printf ("SendConfirmedUnsegmented\n");
                    send_conf_request();
                    osServiceTimerStart(svc, RequestTimeout);
                    tsm->RetryCount=0;
                    state = AWAIT_CONFIRMATION;
                    N_UNITDATA_req(npci, pdu, pdu_len, tsm->apdu, tsm->apdu_len);
                }
            } else
            if (signals & N_UNITDATA_indication) {
                if (tsm->state == AWAIT_CONFIRMATION) {
                    uint8_t pdu_type = pdu[0];
                    switch (pdu_type& 0xFF) {
                    case BACnet_SimpleACK_PDU:// SimpleACK_Received
//                        osServiceTimerStop(svc);
                        //CONF_SERV_cnf(+);
//                        tsm->apdu_len=0;// пусто явным образом
                        //osService_t * application_program = device_confirmed_services[tsm->service_choice].service;
//                        osServiceSignal(&tsm->local_application_program, CONF_SERV_confirm);
                        application_confirm(CONF_SERV_confirm);
                        state = IDLE;
                        break;
                    case BACnet_ComplexACK_PDU:// UnsegmentedComplexACK_Received
//                        osServiceTimerStop(svc);
                        //CONF_SERV_cnf(+);
//                        tsm->apdu_len=0;
//                        osServiceSignal(&tsm->local_application_program, CONF_SERV_confirm);
                        application_confirm(CONF_SERV_confirm);
                        state = IDLE;
                        break;
                    case (BACnet_ComplexACK_PDU|SEG|MOR):// ErrorPDU_Received
                    case (BACnet_ComplexACK_PDU|SEG): {
                        uint8_t seq_number = pdu[3];
                        if (seq_number==0 && npci->segmentation_supported) {// SegmentedComplexACK_Received
                            printf("SegmentedComplexACK_Received\n");
                            state = SEGMENTED_CONF;
                        } else {// UnexpectedPDU_Received
                            printf("UnexpectedPDU_Received\n");
                            // см ниже
                        }
                    } break;
                    case BACnet_Error_PDU:// ErrorPDU_Received
                        osServiceTimerStop(svc);
                        //CONF_SERV_cnf(-);
                        //tsm->service_choice]
                        //osServiceSignal(local->application_program, CONF_SERV_confirm);
                        application_confirm(CONF_SERV_confirm);
                        state = IDLE;
                        break;
                    case BACnet_Reject_PDU:// RejectPDU_Received
                        osServiceTimerStop(svc);
//                        REJECT_ind(device, reason);
//                        osServiceSignal(local->application_program, REJECT_indication);
                        application_indication(REJECT_indication);
                        state = IDLE;
                        break;
                    case (BACnet_Abort_PDU|SRV):// AbortPDU_Received
                        osServiceTimerStop(svc);
//                        ABORT_ind(device, reason);
//                        osServiceSignal(local->application_program, ABORT_indication);
                        application_indication(ABORT_indication);
                        state = IDLE;
                        break;
                    case (BACnet_SegmentACK_PDU|SRV): // SegmentACK_Received
                        // -- then discard the PDU as a duplicate, and re-enter the current state.
                        break;
                    default: // UnexpectedPDU_Received
                        osServiceTimerStop(svc);
                        send_abort(INVALID_APDU_IN_THIS_STATE);
//                        N_UNITDATA_req(npci, BACnet_Abort_PDU, , NPCI_control);

//                        ABORT_ind(, INVALID_APDU_IN_THIS_STATE, /* 'server'= */FALSE);
//                        osServiceSignal(local->application_program, ABORT_indication);
                        application_indication(ABORT_indication);
                        state = IDLE;
                        break;
                    }

                } else
                if (tsm->state == SEGMENTED_REQUEST) {
                    uint8_t pdu_type = pdu[0];
                    switch (pdu_type& 0xF0) {
                    case BACnet_SegmentACK_PDU:
                    case BACnet_ComplexACK_PDU:
                        // DuplicateACK_Received
                        // NewACK_Received
                        // FinalACK_Received
                        break;
                    }
                } else
                {
                    uint8_t pdu_type = pdu[0];
                    switch (pdu_type& 0xF0) {
                    case BACnet_SegmentACK_PDU:

                        // UnexpectedSegmentInfoReceived
                        break;
                    default:
                        // UnexpectedPDU_Received
                        break;
                    }
                }
            } else
            if (signals & N_REPORT_indication) {// SecurityError_Received
                if (tsm->state == SEGMENTED_REQUEST) {
//                    SEC_ERR_ind(device);
//                    osServiceTimerStop(self);
                    application_indication(SEC_ERR_indication);
                }
                state = IDLE;
            } else
            if (signals & ABORT_request) {// SendAbort  from the local application program
//                osServiceTimerStop(svc);
                send_abort(tsm->apdu[0]);
                state = IDLE;
            }
        }
    tsm->state = state;
    if (pdu_len!=0) {
        N_UNITDATA_req(npci, pdu, pdu_len, &tsm->apdu[0], tsm->apdu_len);
    }
    if (state == IDLE) {
        // завершить освободить ресурсы tsm
    }
	return 0;
}
