/*! Есть два уровня поддрежки, на разный тип контроллеров
Этот вариант без фрагментации пакетов
*/
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "bacnet.h"
#include "bacnet_net.h"
#include "bacnet_object.h"
#include "bacnet_error.h"

//#pragma weak bacnet_relay = bacnet_datalink_request

#define PDU_FLAG_SEG 0x8
#define PDU_FLAG_MOR 0x4
#define PDU_FLAG_NAK 0x2
#define PDU_FLAG_SA  0x2
#define PDU_FLAG_SRV 0x1

uint8_t assign_invoke_id()
{
	return 0;
}

/*! \brief запросить отмену обработки данных */
void ABORT_ind(Device_t* dl, uint16_t err)
{

}
void UNCONF_SERV_req(Device_t* device, BACnetAddr_t* remote_device_addr, DataUnit_t* tr)
{
//    tr->status = 0; -- тут передается service_choice
    tr->destination = remote_device_addr;

//    device->apdu_transfer = *tr;
    DataUnitRef(tr);
    osAsyncQueuePut(&device->apdu_requesting_queue,   tr);
    osServiceSignal(device->apdu_requesting_service, UNCONF_SERV_request);
//    osServiceNotify(device->apdu_requesting_service); -- вызывает переход
}


/*! \brief запросить передачу данных от службы к сетевому уровню */
static int N_UNITDATA_req(Device_t* device, BACnetNPCI* npci, struct _NPDU* pdu, bool data_expecting_reply)
{
	DataUnit_t * tr = osAsyncQueueGet(&device->apdu_requesting_queue);//&device->apdu_transfer;//&transfer_apdu2mstp;
	if (tr==NULL) {
        printf("ERROR: assert osAsyncQueueGet == NULL\n");
        return -1;
	}

	uint8_t* data = tr->buffer;
	switch (pdu->type & 0xF0) {
	case BACnet_Confirmed_Request_PDU:
		*data++ = pdu->type;
		*data++ = pdu->maxsegs;
		// Max Segs = (0..7) (Number of response segments accepted per Clause 20.1.2.4)
		// Max Resp = (0..15) (Size of Maximum APDU accepted per Clause 20.1.2.5)
		*data++ = pdu->invoked_id;// invoke-id
		if (pdu->type & PDU_FLAG_SEG) {
			*data++ = pdu->sequence_number;	//
			*data++ = pdu->window_size;
		}
		*data++ = pdu->service_choice;
		// Service Request = Variable Encoding per Clause 20.2
		break;
	case BACnet_Unconfirmed_Request_PDU:
	    tr->buffer -=2; tr->size+=2;
	    data = tr->buffer;
		*data++ = pdu->type;
		*data++ = pdu->service_choice;
		// Service Request = Variable Encoding per Clause 20.2.
		break;
	case BACnet_SimpleACK_PDU:
		*data++ = pdu->type;
		*data++ = pdu->invoked_id;// original-invoke-id
		*data++ = pdu->service_choice;	// BACnetConfirmedServiceChoice
		break;
	case BACnet_ComplexACK_PDU:
		*data++ = pdu->type;
		*data++ = pdu->invoked_id;// original-invoke-id
		if (pdu->type & PDU_FLAG_SEG) {
			*data++ = pdu->sequence_number;	//
			*data++ = pdu->window_size;
		}
		*data++ = pdu->service_choice;	// BACnetConfirmedServiceChoice
		// Service ACK = Variable Encoding per Clause 20.2.
		break;
	case BACnet_SegmentACK_PDU:
		*data++ = pdu->type;
		*data++ = pdu->invoked_id;// original-invoke-id
		*data++ = pdu->sequence_number;	//
		*data++ = pdu->window_size;
		break;
	case BACnet_Error_PDU:
		*data++ = pdu->type;
		*data++ = pdu->invoked_id;// original-invoke-id
		*data++ = pdu->service_choice;	// BACnetConfirmedServiceChoice
		// Error = Variable Encoding per Clause 20.2.
		break;
	case BACnet_Reject_PDU:
		*data++ = pdu->type;
		*data++ = pdu->invoked_id;// original-invoke-id
		*data++ = pdu->service_choice;	// BACnetRejectReason -- One octet containing the reject reason enumeration
		break;
	case BACnet_Abort_PDU:
		*data++ = pdu->type;
		*data++ = pdu->invoked_id;// original-invoke-id
		*data++ = pdu->service_choice;	// BACnetAbortReason
		break;
	}
	//tr->size = (data - tr->buffer)+size;
	npci->SA.len=3; npci->SA.vmac_addr = device->instance.oid.instance;
    npci->SA.network_number=0;// не знаю какой
    if (tr->destination)
        npci->DA = *(BACnetAddr_t*)tr->destination;
    else {
        npci->DA.network_number = GLOBAL_BROADCAST;
        npci->DA.len = 0;
    }
    npci->control = 0;//0x28;
    int res = bacnet_relay(device->network_ports, npci, tr);// weak=bacnet_datalink_request - когда функция не определена
    DataUnitUnref(tr);// это должно происходить в той же функции что и osAsyncQueueGet
    return res;
//	bacnet_datalink_request(dl, npci, tr);
}
/*! \brief запросить обработку данных от сетевого уровня к прикладному (apdu) */
void N_UNITDATA_ind(Device_t* device, uint8_t type,  uint8_t *data, size_t size)
{
//	DataUnit_t * tr = dl->transfer;

	osServiceSignal(device->apdu_responding_service, N_UNITDATA_indication);
	osServiceNotify(device->apdu_responding_service);
}
void SEC_ERR_ind(Device_t* dl)
{

}
/*! The N-REPORT.indication primitive is used by the local network layer to indicate failures to transmit NUNITDATA.requests to peer devices. The errors may be locally detected error conditions, or error conditions reported by a
peer device via a network layer message. This primitive is used extensively by the network security wrapper to indicate
security errors up the stack. The 'peer_address' parameter is of the same form as the 'destination_address' or 'source_address'
parameters of the N-UNITDATA primitives and indicates the peer with which the error condition arose. The optional
parameter 'security_parameters' conveys information describing the security failure and context required to relate the error to
a previous N-UNITDATA.request or N-UNITDATA.indication primitive.
*/
void N_REPORT_ind(Device_t* dl, uint8_t err_condition,  uint16_t error_parameters)
{
	osServiceSignal(dl->apdu_requesting_service, N_REPORT_indication);
//	osServiceNotify(dl->apdu_requesting_service);
}

#define BROADCAST 0xFF
#define SEGMENTED_NOT_SUPPORTED 1
#define SEGMENTED_TRANSMISSION_SUPPORTED 0
static const size_t   maximum_transmittable_length=502;
static const uint32_t RequestTimeout = 100;
static const uint32_t SegmentTimeout = 100;
static const uint8_t  Max_Segments_Accepted=3;
static const uint8_t  Number_Of_APDU_Retries=1;
static const uint8_t  N_retry=2;

//DataUnit_t transfer_mstp2apdu;
//DataUnit_t transfer_apdu2mstp;
/*
static const osSignalMap_t signals[] = {
		{  MSTP_SERVICE, (N_UNITDATA_request|CONF_SERV_response)},
		{DEVICE_SERVICE, (CONF_SERV_request|CONF_SERV_response)}
	};*/
//! \brief 5.4.1 Variables And Parameters
typedef struct _TransactionStateMachine TSM_t;
struct _TransactionStateMachine {
    uint8_t RetryCount;         //!< used to count APDU retries
    uint8_t SegmentRetryCount;  //!< used to count segment retries
    uint8_t DuplicateCount;     //!< used to count duplicate segments
    bool SentAllSegments;       //!< used to control APDU retries and the acceptance of server replies
    uint8_t LastSequenceNumber; //!< stores the sequence number of the last segment received in order
    uint8_t InitialSequenceNumber;//!< stores the sequence number of the first segment of a sequence of segments that fill a window
    uint8_t ActualWindowSize;   //!< stores the current window size
    uint8_t ProposedWindowSize; //!< stores the window size proposed by the segment sender
//    uint64_t SegmentTimer;      //!< used to perform timeout on PDU segments
//    uint64_t RequestTimer;      //!< used to perform timeout on Confirmed Requests
};
struct _apdu_ctx {
    osService_t*service;
    Device_t*   device;
//	DataLink_t *dl;// обратная ссылка на интерфейс
	DataUnit_t transfer_mstp2apdu;
//	DataUnit_t *transfer_apdu2dev;
	enum _APDU_state {
		APDU_IDLE,
		SEGMENTED_REQUEST,

		AWAIT_CONFIRMATION,
		SEGMENTED_CONF,

		AWAIT_RESPONSE,
		SEGMENTED_RESPONSE,
	} state;
	struct _TransactionStateMachine tsm;
	BACnetNPCI npci;
};

int bacnet_apdu_responding_service (void * service_data);
int bacnet_apdu_requesting_service (void * service_data);
struct _apdu_ctx* bacnet_apdu_init (Device_t* device)
{
	osService_t* svc;
	{
        struct _apdu_ctx * ctx = malloc(sizeof(struct _apdu_ctx));
        svc = osServiceCreate(NULL, bacnet_apdu_responding_service, ctx);
        ctx->service = svc;
        ctx->device  = device;
        ctx->state   = APDU_IDLE;
        device->apdu_responding_service = svc;
	}
//	ctx->dl = NULL;
    {
        struct _apdu_ctx * ctx = malloc(sizeof(struct _apdu_ctx));
        svc = osServiceCreate(NULL, bacnet_apdu_requesting_service, ctx);// свой контекст?
        ctx->service = svc;
        ctx->device  = device;
        ctx->state   = APDU_IDLE;
        device->apdu_requesting_service	= svc;

    }


// У объекта служба может быть таблица сигналов
//	osServiceTimerCreate(svc, RequestTimeout);
	osServiceRun(device->apdu_responding_service);
	osServiceRun(device->apdu_requesting_service);
// У объекта служба может быть таблица виртуальных функций!
    return NULL;
}

#define SEQ_MASK 0xFF
/*! \brief 5.4.2.1 Function InWindow

The function "InWindow" performs a modulo 256 compare of two unsigned eight-bit sequence numbers. All computations
and comparisons are modulo 256 operations on unsigned eight-bit quantities.
*/
static inline bool InWindow(uint8_t seqA, uint8_t seqB, uint8_t ActualWindowSize)
{
    return (uint8_t)((seqA-seqB)&SEQ_MASK) < tsm->ActualWindowSize;
}
/*! \brief 5.4.2.2 Function DuplicateInWindow

The function "DuplicateInWindow" determines whether a value, seqA, is within the range firstSeqNumber through
lastSequenceNumber, modulo 256, or if called at the start of a new Window and no new message segments have been
received yet, it determines if the value seqA is within the range of the previous Window. All computations and comparisons
are modulo 256 operations on unsigned eight-bit quantities.
*/
static inline bool DuplicateInWindow(uint8_t seqA, uint8_t firstSeqNumber, uint8_t lastSequenceNumber, uint8_t ActualWindowSize) {
	uint8_t receivedCount = (lastSequenceNumber - firstSeqNumber)&SEQ_MASK;
	if (receivedCount > ActualWindowSize) return false;
	if ((uint8_t)((seqA - firstSeqNumber)&SEQ_MASK) <= receivedCount) return true;
	if (receivedCount==0 && (uint8_t)((firstSeqNumber - seqA)&SEQ_MASK) <= ActualWindowSize) return true;
	return false;
}
/*! \brief 5.4.3 Function FillWindow

The function "FillWindow" sends PDU segments either until the window is full or until the last segment of a message has
been sent. No more than T_seg may be allowed to elapse between the receipt of a SegmentACK APDU and the transmission of
a segment. No more than T_seg may be allowed to elapse between the transmission of successive segments of a sequence
*/

// переделать!!!
static bool FillWindow(Device_t* device, BACnetNPCI *npci, TSM_t *tsm, struct _NPDU * pdu/*, uint8_t sequenceNumber*/)
{
    uint8_t sequenceNumber = tsm->InitialSequenceNumber;
    uint8_t ix;
    const size_t max_pdu_length =maximum_transmittable_length;
    size_t offset = sequenceNumber*max_pdu_length;
	DataUnit_t* tr = &device->apdu_transfer;
//	uint8_t* data  = tr->buffer;
	size_t data_length = tr->size;

    for(ix=0; (offset+max_pdu_length) < data_length; offset+=max_pdu_length) {
        pdu->type = (pdu->type&0xF0) | (PDU_FLAG_SEG|PDU_FLAG_MOR);// SEG MOR  'segmented-message' = TRUE, 'more-follows' = TRUE
        pdu->window_size = tsm->ProposedWindowSize; // 'proposed-window-size' equal to ProposedWindowSize
        pdu->sequence_number = sequenceNumber+ix; //data_expecting_reply =
		//&data[offset], max_pdu_length -- \todo копировать данные в буфер
        N_UNITDATA_req(device, npci, pdu, true);//  'data_expecting_reply' = TRUE
        if ((++ix) == tsm->ActualWindowSize)
            return tsm->SentAllSegments = false;
    }
    pdu->type = (pdu->type&0xF0) | PDU_FLAG_SEG;
    pdu->window_size = tsm->ProposedWindowSize; // 'proposed-window-size' equal to ProposedWindowSize
    pdu->sequence_number = sequenceNumber+ix;
	// &data[offset], data_length-offset -- \todo копировать данные в буфер
	N_UNITDATA_req(device, npci, pdu, true);//  'data_expecting_reply' = TRUE
    return tsm->SentAllSegments = true;
}

int bacnet_apdu_responding_service (void * service_data)
{
	struct _apdu_ctx * ctx = service_data;
	osService_t *self = ctx->service;//osServiceGetId();
    osEvent* event = osServiceGetEvent(self);
/* APDUs sent by responding BACnet-users (servers):

BACnet-SimpleACK-PDU
BACnet-ComplexACK-PDU
BACnet-Error-PDU
BACnet-Reject-PDU
BACnet-SegmentACK-PDU with 'server' = TRUE
BACnet-Abort-PDU with 'server' = TRUE
*/
	struct _NPDU pdu;// = {0};
	int state = ctx->state;
//	DataLink_t* dl = ctx->dl;
    Device_t* device = ctx->device;
	BACnetNPCI* npci=&ctx->npci;
	if (event->status & osEventTimeout) {// If RequestTimer becomes greater than Tout,
		pdu.type = (BACnet_Abort_PDU|PDU_FLAG_SRV);
		pdu.service_choice = ABORT_APPLICATION_EXCEEDED_REPLY_TIME;
		// отослать на специальном буфере, маленьком, потому что не требуется ответ
		N_UNITDATA_req(device, npci, &pdu, false);
		ABORT_ind  (device, ABORT_APPLICATION_EXCEEDED_REPLY_TIME);
		// очистили флаги ждем
	} else
	if (event->status & osEventSignal) {
		uint32_t signals = event->value.signals;
		if (signals & CONF_SERV_response) {// пришел ответ от прикладного уровня
			DataUnit_t *tr = &device->apdu_transfer;
			if (tr->status<0) {// ответ отрицательный надо ошибку оформить
				pdu.type = BACnet_Error_PDU;
				N_UNITDATA_req(device, npci, &pdu, false);
			} else
			if (tr->size > 0) {// SendUnsegmentedComplexACK
				if (tr->size > maximum_transmittable_length) {// 502
					if (SEGMENTED_TRANSMISSION_SUPPORTED) {

					} else {
						pdu.type = (BACnet_Abort_PDU|PDU_FLAG_SRV);
						pdu.service_choice = ABORT_SEGMENTATION_NOT_SUPPORTED;
						N_UNITDATA_req(device, npci, &pdu, false);
					}
				} else {
					pdu.type = BACnet_ComplexACK_PDU;
					N_UNITDATA_req(device, npci, &pdu, false);
				}
			} else {// SendSimpleACK
				pdu.type = BACnet_SimpleACK_PDU;
				N_UNITDATA_req(device, npci, &pdu, false);
			}
		} else
		if (signals & N_UNITDATA_indication) {
			DataUnit_t *tr = &device->apdu_transfer;// \todo сделать частью сервиса
			uint8_t * data = tr->buffer;
			pdu.type = *data++;

			switch (pdu.type & 0xF0) {
			case BACnet_Unconfirmed_Request_PDU: {
				pdu.service_choice = *data++;
				tr->buffer+=2, tr->size-=2;
				UNCONF_SERV_ind(device, pdu.service_choice, tr);
				state = APDU_IDLE;
			} break;
			case BACnet_Confirmed_Request_PDU:
                pdu.invoked_id = *data++;
				pdu.service_choice = *data++;
				if (tr->destination == NULL/* BROADCAST */) {// ConfirmedBroadcastReceived игнорируем
					state = APDU_IDLE;
				} else
				if (pdu.type & PDU_FLAG_SEG) {
					if (SEGMENTED_NOT_SUPPORTED) {// ConfirmedSegmentedReceivedNotSupported
						pdu.type = (BACnet_Abort_PDU|PDU_FLAG_SRV);
						pdu.service_choice = ABORT_SEGMENTATION_NOT_SUPPORTED;
						N_UNITDATA_req(device, npci, &pdu, false);
					} else {
						// ConfirmedSegmentedReceivedWindowSizeOutOfRange
						// ConfirmedSegmentedReceived
					}
				} else {// ConfirmedUnsegmentedReceived
					CONF_SERV_ind(device, pdu.service_choice, data, tr->size-2);
					osServiceTimerStart(self, RequestTimeout);// \todo проверить логику чтобы таймер отанавливали
					state = AWAIT_RESPONSE;
				}
				break;
			case BACnet_Abort_PDU:
				if ((pdu.type & PDU_FLAG_SRV)==0) {
					state = APDU_IDLE;
				}
				break;
			default:// слать лесом не поддерживаем!
				pdu.type = (BACnet_Abort_PDU|PDU_FLAG_SRV);
				pdu.service_choice = OTHER;
				N_UNITDATA_req(device, npci, &pdu, false);
				break;
			}
		} else
		if (signals & N_REPORT_indication) {// SecurityError_Received
			osServiceTimerStop(self);
			state = APDU_IDLE;
		} else
		if (signals & N_REPORT_indication) {// SendAbort
			osServiceTimerStop(self);
			pdu.type = (BACnet_Abort_PDU|PDU_FLAG_SRV);
			pdu.service_choice = OTHER;
			N_UNITDATA_req(device, npci, &pdu, false);
			state = APDU_IDLE;
		}
	}
	ctx->state=state;
	return 0;
}


#define g_slice_dup(arg) __builtin_memcpy(g_slice_alloc(sizeof(arg)), &(arg), sizeof(arg))
/// npci - часть контекста _apdu_ctx
int bacnet_apdu_service(DataLink_t* dl, BACnetNPCI *npci, uint8_t * buffer, size_t size /* DataUnit_t* transfer*/)
{
//    struct _apdu_ctx *ctx = dl->apdu;
//    DataUnit_t* transfer = &ctx->transfer_mstp2apdu;//g_slice_alloc(sizeof(DataUnit_t));//ctx->transfer_mstp2apdu;// может создать новый?
//    transfer->status = npci->control;

//    transfer->destination = &npci->SA;/// \todo сделать иначе
//    transfer->size   = size;
    Device_t* device = dl->device;
//    device->apdu_responding_transfer = transfer;
if (device->apdu_transfer.refcount!=0) printf("%s: collision\n", __FUNCTION__);
    device->apdu_transfer.buffer = transfer->buffer;
    device->apdu_transfer.size   = transfer->size;
    device->apdu_transfer.status = npci->control;
printf("%s:", __FUNCTION__);
    BACnetAddr_t *source_addr = g_slice_alloc(sizeof(BACnetAddr_t));
    *source_addr = npci->SA;
    device->apdu_transfer.destination = source_addr;/// компу принадлежит структура?
    device->apdu_transfer.refcount=1;
///    device->apdu_transfer_source_address = npci->SA;

//    device->apdu_responding_
    osServiceSignal(device->apdu_responding_service, N_UNITDATA_indication);
//    osServiceNotify(device->apdu_responding_service);
    return 0;//bacnet_apdu_responding_service(apdu);
}
/*! 5.4.4 State Machine for Requesting BACnet User (client)

*/
int bacnet_apdu_requesting_service (void * service_data)
{
	struct _apdu_ctx  *ctx = service_data;
	osService_t *self = ctx->service;//osServiceGetId();
    osEvent* event = osServiceGetEvent(self);
	struct _NPDU pdu;
	Device_t* device = ctx->device;
//	DataLink_t* dl = ctx->dl;
	TSM_t * tsm = &ctx->tsm;
	int state = ctx->state;
	if (event->status & osEventTimeout) {
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
		} else
		if (state == AWAIT_CONFIRMATION) {
			DataUnit_t* tr = &device->apdu_transfer;
			if (tsm->RetryCount >= Number_Of_APDU_Retries) { // FinalTimeout
				osServiceTimerStop(self);
				ABORT_ind(device, ABORT_TSM_TIMEOUT);
				state = APDU_IDLE;
			} else
			if (tr->size > maximum_transmittable_length) { // TimeoutSegmented
				osServiceTimerStop(self);
				tsm->RetryCount++;
				tsm->SegmentRetryCount=0, tsm->SentAllSegments=false,
				tsm->InitialSequenceNumber=0, tsm->ActualWindowSize=1;
				osServiceTimerStart(self, SegmentTimeout);
				pdu.type = (BACnet_Confirmed_Request_PDU|PDU_FLAG_SEG|PDU_FLAG_MOR);
				pdu.sequence_number=0;
				N_UNITDATA_req(device, &ctx->npci, &pdu, true);
				state = SEGMENTED_REQUEST;
			} else {// TimeoutUnsegmented
				osServiceTimerStop(self);
				tsm->RetryCount++;
				osServiceTimerStart(self, RequestTimeout);
				pdu.type = BACnet_Confirmed_Request_PDU;
				N_UNITDATA_req(device, &ctx->npci,&pdu, true);
				state = AWAIT_CONFIRMATION;
			}
		}
	} else
	if (event->status & osEventSignal) {
		uint32_t signals = event->value.signals;
		if (signals &  UNCONF_SERV_request) {// SendUnconfirmed
			// If UNCONF_SERV.request is received from the local application program,
			pdu.type = BACnet_Unconfirmed_Request_PDU;
			pdu.service_choice = device->apdu_requesting_transfer.status;
            //dl->app_transfer = &ctx->device->apdu_transfer;
			N_UNITDATA_req(device, &ctx->npci,&pdu, false);//
			state = APDU_IDLE;
		} else
		if (signals & CONF_SERV_request) {
			// from the local application program,
			DataUnit_t* tr=&device->apdu_transfer;
			if (tr->size > maximum_transmittable_length) {//  as determined according to Clause 5.2.1
			// Max_APDU_Length_Accepted property of the remote peer's Device object.
				if (Max_Segments_Accepted ==0
				|| (Max_Segments_Accepted * maximum_transmittable_length) < tr->size) {// CannotSend
					ABORT_ind(device, ABORT_APDU_TOO_LONG);
					state = APDU_IDLE;
				} else {// SendConfirmedSegmented
					pdu.invoked_id = assign_invoke_id();
				}
			} else {// SendConfirmedUnsegmented
//				dl->data_expecting_reply = true;
				pdu.type = BACnet_Confirmed_Request_PDU;
				pdu.invoked_id = assign_invoke_id();
				//{.SentAllSegments = true, .RetryCount=0, };
				osServiceTimerStart(self, RequestTimeout);
				state = AWAIT_CONFIRMATION;
			}
		} else
		if (signals & N_UNITDATA_indication) {
			DataUnit_t*tr = &device->apdu_transfer;// это другой трансфер??
			uint8_t *data = tr->buffer;
			if (state == SEGMENTED_REQUEST) {
				pdu.type = *data++;
				switch (pdu.type& 0xF0) {
				case BACnet_SegmentACK_PDU:
					// DuplicateACK_Received
					// NewACK_Received
					// FinalACK_Received
					break;
				}
			} else  {
				pdu.type = *data++;
				switch (pdu.type& 0xF0) {
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
			if (state == SEGMENTED_REQUEST) {
				SEC_ERR_ind(device);
				osServiceTimerStop(self);
			}
			state = APDU_IDLE;
		} else
		if (signals & ABORT_request) {// SendAbort  from the local application program
			osServiceTimerStop(self);
			pdu.type = BACnet_Abort_PDU;
			pdu.service_choice = OTHER;
			N_UNITDATA_req(device, &ctx->npci, &pdu, false); // data_expecting_reply = false;
			state = APDU_IDLE;
		}
	}
	ctx->state = state;
	return 0;
}

