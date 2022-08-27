#include <stdio.h>
#include <string.h>
#include "bacnet_net.h"
#include "bacnet_services.h"

// "bacnet_apdu.h"
enum _APDU_state {
	IDLE, SEGMENTED_REQUEST, SEGMENTED_RESPONSE, AWAIT_RESPONSE
};
enum _BACnetRejectReason /* ENUMERATED */{
//OTHER =(0),
//BUFFER_OVERFLOW =(1),
INCONSISTENT_PARAMETERS =(2),
INVALID_PARAMETER_DATA_TYPE =(3),
INVALID_TAG =(4),
MISSING_REQUIRED_PARAMETER =(5),
PARAMETER_OUT_OF_RANGE =(6),
TOO_MANY_ARGUMENTS =(7),
UNDEFINED_ENUMERATION =(8),
UNRECOGNIZED_SERVICE =(9),
// ...
};
// -- Enumerated values 0-63 are reserved for definition by ASHRAE. Enumerated values 64-255
// -- may be used by others subject to the procedures and constraints described in Clause 23.

enum _BACnetAbortReason /* ENUMERATED */{
OTHER 	=(0),
BUFFER_OVERFLOW 	=(1),
INVALID_APDU_IN_THIS_STATE 	=(2),
PREEMPTED_BY_HIGHER_PRIORITY_TASK 	=(3),
SEGMENTATION_NOT_SUPPORTED 	=(4),
SECURITY_ERROR 	=(5),
INSUFFICIENT_SECURITY 	=(6),
WINDOW_SIZE_OUT_OF_RANGE 	=(7),
APPLICATION_EXCEEDED_REPLY_TIME 	=(8),
OUT_OF_RESOURCES 	=(9),
TSM_TIMEOUT 	=(10),
APDU_TOO_LONG 	=(11),
// ...
};

// -- Enumerated values 0-63 are reserved for definition by ASHRAE. Enumerated values 64-255
// -- may be used by others subject to the procedures and constraints described in Clause 23.
/* 20.1.2.5 max-apdu-length-accepted
B'0000' Up to MinimumMessageSize (50 octets)
B'0001' Up to 128 octets
B'0010' Up to 206 octets (fits in a LonTalk frame)
B'0011' Up to 480 octets (fits in an ARCNET frame)
B'0100' Up to 1024 octets
B'0101' Up to 1476 octets (fits in an Ethernet frame)
*/
static uint32_t max_response_size(uint32_t device_max_apdu, uint8_t max_resp)
{
	static const uint16_t max_apdu[] = {50, 128, 206, 480, 1024, 1476};
//	int bits = (max_resp>>4)&0x7;
	int max_apdu_idx = max_resp&0xF;
	return  (max_apdu_idx>5)? device_max_apdu: max_apdu[max_apdu_idx];
}

#define SEG 0x8
#define MOR 0x4
#define NAK 0x2
#define SA  0x2
#define SRV 0x1
#define SEQ_MASK 0xFF // это маска для modulo 256

#define DATA_EXPECTING_REPLY 0x04 // бит 3 в NPDU.control

struct _APDU_Ctx {
	enum _APDU_state state;
	size_t seq_pos;// последовательная позиция чтения из буфера APDU
	size_t total_length;// полная длина для фрагментированных пакетов 
	uint16_t MTU;
	uint8_t seq_number_prev;// InitialSequenceNumber
	uint8_t seq_number_last;// LastSequenceNumber
	uint8_t ActualWindowSize;	// ActualWindowSize
	uint8_t DuplicateCount;	// ActualWindowSize
	
};

BACnetConfService_t device_confirmed_services[32];
BACnetService_t device_unconfirmed_services[16];

/*! \brief 5.4.2.1 Function InWindow

The function "InWindow" performs a modulo 256 compare of two unsigned eight-bit sequence numbers. All computations
and comparisons are modulo 256 operations on unsigned eight-bit quantities.
*/
static inline bool InWindow(uint8_t seqA, uint8_t seqB, uint8_t ActualWindowSize)
{
    return (uint8_t)((seqA-seqB)&SEQ_MASK) < ActualWindowSize;
}
/*! \brief 5.4.2.2 Function DuplicateInWindow

The function "DuplicateInWindow" determines whether a value, seqA, is within the range firstSeqNumber through
lastSequenceNumber, modulo 256, or if called at the start of a new Window and no new message segments have been
received yet, it determines if the value seqA is within the range of the previous Window. All computations and comparisons
are modulo 256 operations on unsigned eight-bit quantities.
*/
static inline bool DuplicateInWindow(uint8_t seqA, uint8_t firstSeqNumber, const uint8_t lastSequenceNumber, const uint8_t ActualWindowSize) {
	uint8_t receivedCount = (lastSequenceNumber - firstSeqNumber)&SEQ_MASK;
	if (receivedCount > ActualWindowSize) return false;
	if ((uint8_t)((seqA - firstSeqNumber)&SEQ_MASK) <= receivedCount) return true;
	if (receivedCount==0 && (uint8_t)((firstSeqNumber - seqA)&SEQ_MASK) <= ActualWindowSize) return true;
	return false;
}


static struct _APDU_Ctx ctx;

#if 0
void bacnet_apdu_slave_n_report()
{
	if (security_error) // SecurityError_Received
		ctx.state=IDLE;
}
#endif

#define DEVICE_SEG_NOT_SUPPORTED 0
#define DEVICE_MAX_APDU_LENGTH 128
static const size_t   maximum_transmittable_length=502;// 50 128 
static const uint32_t RequestTimeout = 100;
static const uint32_t SegmentTimeout = 100;
//static const uint8_t  max_segments_accepted=3;
static const uint8_t  Number_Of_APDU_Retries=1;
static const uint8_t  N_retry=2;
static const uint8_t  Ndup=2;

static uint8_t apdu_buffer[1024];
#define BROADCAST 0xFF

// Данные отсылаются этой функцией
void N_UNITDATA_req(BACnetNPCI * npci, uint8_t* buffer, uint16_t size, uint8_t data_expecting_reply)
{
	// дописать шапку, шапку дописывает фукнция bacnet_relay
	buffer-=2;
	buffer[0]=0x01;// version
	buffer[1]=data_expecting_reply?0x04:0x0;// local
	struct _DL_hdr * dl=(struct _DL_hdr *)npci;
	dl->data = buffer;// скопировали шапку
	dl->data_len = size+2;
	if (dl->owner) osSignalSet(dl->owner, DL_UNITDATA_request);// к пакету не дописана шапка NPDU 
}
/*
*/
int CONF_SERV_ind(BACnetNPCI* npci, uint8_t service_choice, uint8_t *apdu_buffer, size_t apdu_length)
{
	struct _DL_hdr* dl = (struct _DL_hdr*)npci;
	npci->await_response=false;// 14.10.2019
	uint8_t data_expecting_reply=0;
	BACnetConfService_t *service = &device_confirmed_services[service_choice];
	if (service==NULL || service->indication==NULL) {
		printf("CONF_SERV_ind UNRECOGNIZED_SERVICE\r\n");
		return -UNRECOGNIZED_SERVICE;// ошибка
	}
	int result = service->indication(npci, apdu_buffer, apdu_length);
	return result;
#if 0
	uint8_t * buf = apdu_buffer-=3; apdu_length+=3;
	if (result==0) {
		printf("BACnet_SimpleACK_PDU\r\n");
		buf[0] = BACnet_SimpleACK_PDU;
	} else
	if (result<0) {
		buf[0] = BACnet_Error_PDU;
	} else
	/* if (result>0) */{
		buf[0] = BACnet_ComplexACK_PDU;
	}
	buf[1] = dl->apdu.invoke_id;
	buf[2] = service_choice;
	data_expecting_reply = 0;
	N_UNITDATA_req(npci, apdu_buffer, apdu_length, data_expecting_reply);
	return -1;
#endif
}
void UNCONF_SERV_ind(BACnetNPCI* npci, uint8_t service_choice, uint8_t *apdu_buffer, size_t apdu_length)
{
	npci->await_response=false;
	if (service_choice >=16) return;
	BACnetService_t *service = &device_unconfirmed_services[service_choice];
	if (service->indication ==NULL ) return;
	service->indication(npci, apdu_buffer, apdu_length);
//	printf("Service found\n");
}

/*!	\brief Сервис BACnet APDU 
	\param npci - Network Protocol Control Interface 
	
 */
int bacnet_apdu_service(DataLink_t* dli, BACnetNPCI * npci, uint8_t * buf, size_t length)
{
	struct _DL_hdr* dl = (struct _DL_hdr*) npci;
    uint8_t pdu_type = buf[0];
	uint8_t invoke_id;
	uint8_t seq_number;
	uint8_t window_size;
	uint8_t service_choice;
//	printf("pdu Type=%d\r\n", pdu_type);
//	return 0;
// nested functions
	int send_abort(uint8_t reason) {
		ctx.state=IDLE;
		buf[0] = BACnet_Abort_PDU|SRV;
		buf[1] = invoke_id;
		buf[2] = reason;// abort reason WINDOW_SIZE_OUT_OF_RANGE
		return 3;
	}
	int send_reject(uint8_t reason) {
		buf[0] = BACnet_Reject_PDU;
		buf[1] = invoke_id;
		buf[2] = reason;// abort reason WINDOW_SIZE_OUT_OF_RANGE
		return 3;
	}
	int send_error(uint32_t error) {
		buf[0] = BACnet_Error_PDU;
		buf[1] = invoke_id;
		buf[2] = service_choice;
		buf[3] = 0x91;
		buf[4] = (error>>16) & 0xFF;
		buf[5] = 0x91;
		buf[6] = error & 0xFF;
		printf("APDU: Send Error PDU\r\n");
		// может быть дополнительные данные
		return 7;
	}
	int send_simpleAck(){
		buf[0] = BACnet_SimpleACK_PDU;
		buf[1] = invoke_id;
		buf[2] = service_choice;
		npci->control &= ~DATA_EXPECTING_REPLY;
		return 3;
	}
	int send_segmentAck(uint8_t pdu_type){
		buf[0] = BACnet_SegmentACK_PDU|pdu_type;
		buf[1] = invoke_id;
		buf[2] = ctx.seq_number_last;
		buf[3] = ctx.ActualWindowSize;
		return 4;
	}
	int send_resp(uint8_t *apdu, int length){// оптравляем один пакет с ожиданием от приложения
		uint8_t * buf = apdu-3;
		buf[0] = BACnet_ComplexACK_PDU;
		buf[1] = invoke_id;
		buf[2] = service_choice;
		npci->control &= ~DATA_EXPECTING_REPLY;// = false;
		N_UNITDATA_req (npci, apdu-3, length+3, 0);
		return -1;
	}
	int send_seg_resp(uint8_t *apdu, int length, size_t ActualWindowSize){
		uint8_t * buf = apdu-5;
		buf[0] = BACnet_ComplexACK_PDU|SEG;
		buf[1] = invoke_id;
		buf[2] = ctx.seq_number_last;
		buf[3] = ActualWindowSize;
		buf[4] = service_choice;
/*		
		while (ctx.MTU < length) {// последний сегмент
			buf[0] = BACnet_ComplexACK_PDU|SEG|MOR;
			N_UNITDATA_req (npci, buf, apdu, ctx.MTU);
			//apdu += MTU, length-=MTU;
			if (--ActualWindowSize) return false; // не все сегменты ососланы
			buf[2] = (++seq_number) & SEQ_MASK;
		}*/
//		buf[0] = BACnet_ComplexACK_PDU|SEG;
		npci->response.MTU = ctx.MTU;
		if (length > ctx.MTU) length = ctx.MTU;
		npci->control |= DATA_EXPECTING_REPLY;// = true;
		N_UNITDATA_req (npci, apdu, length, 1/*,  data_expecting_reply TRUE*/);
		return 5;// размер шапки
	}
	void save_segment(uint8_t *buf, int length) {
		ctx.seq_number_last = seq_number;
		memcpy(&apdu_buffer[ctx.seq_pos], buf, length);
		ctx.seq_pos += length;
	}
// !nested functions
    switch (pdu_type& 0xF0) {
    case BACnet_Confirmed_Request_PDU: {
		if (npci->DA.mac[0]==BROADCAST) { //ConfirmedBroadcastReceived
			ctx.state=IDLE; 
			return 0; 
		}
		int offset=1;// пропустили два поля 20.1.2.4 max-segments-accepted  и 20.1.2.5 max-apdu-length-accepted
		dl->apdu.max_response= buf[offset++]; 
		dl->apdu.invoke_id = invoke_id = buf[offset++]; 
		
        if ((pdu_type & (SEG|MOR)) ==(SEG|MOR)) {
			seq_number  = buf[offset++];
			dl->apdu.window_size = window_size = buf[offset++];
			service_choice = buf[offset++];
			if (ctx.state != SEGMENTED_REQUEST) {
				if (seq_number!=0) {// UnexpectedPDU_Received
				// освободить очередь входную, чтобы не выдавать много абортов
					return send_abort(INVALID_APDU_IN_THIS_STATE);
				}
				if (!(0<window_size && window_size<=127)) {// ConfirmedSegmentedReceivedWindowSizeOutOfRange
					return send_abort(WINDOW_SIZE_OUT_OF_RANGE);
				}
				ctx.state = SEGMENTED_REQUEST;
				ctx.seq_pos = 0;
				ctx.ActualWindowSize = 1;//compute ActualWindowSize based on the 'proposedwindow-size' 
				save_segment(buf+offset, length-offset);
				return send_segmentAck(0);
			} else 
			if ((ctx.seq_number_last+1)&SEQ_MASK == seq_number) {
				save_segment(buf+offset, length-offset);
				if ((ctx.seq_number_prev+ctx.ActualWindowSize)&SEQ_MASK != seq_number) {// NewSegmentReceived
					return -1;// не отсылать
				} else {// LastSegmentOfGroupReceived
					return send_segmentAck(0);
				}
			} else 
			if (DuplicateInWindow(seq_number, ctx.seq_number_prev+1, ctx.seq_number_last, ctx.ActualWindowSize)
				&& ctx.DuplicateCount < Ndup) {// DuplicateSegmentReceived
				ctx.DuplicateCount++;
				return -1;// не отсылать
			} else {
				ctx.DuplicateCount=0;
				return send_segmentAck(NAK);
			}
		} else 
        if ((pdu_type & (SEG|MOR)) ==(SEG)) {// LastSegmentOfMessageReceived
			seq_number  = buf[offset++];
			dl->apdu.window_size = window_size = buf[offset++];
			service_choice = buf[offset++];
			if (ctx.state != SEGMENTED_REQUEST) {
				return send_abort(INVALID_APDU_IN_THIS_STATE);
			}
			if ((ctx.seq_number_last+1)&SEQ_MASK != seq_number) {
				return send_segmentAck(NAK);
			}// LastSegmentOfMessageReceived
			save_segment(buf+offset, length-offset);
			ctx.state = AWAIT_RESPONSE;
			int res = CONF_SERV_ind(npci, service_choice, apdu_buffer, ctx.seq_pos);// service_run()
			if (res>=0) return send_reject(res);
			// See Clause 9.8 for specific considerations in MS/TP networks
			// Wait (Tout)-- This parameter represents the value of the APDU_Timeout property of the node's Device object.
/*			PostponeReply
If a CONF_SERV.response will not be received from the local application layer early enough that a reply MS/TP
frame would be received by the remote node within Treply_timeout (defined in Clause 9.5.3) after the transmission of the
original BACnet-Confirmed-Request-PDU (the means of this determination are a local matter),
then direct the MS/TP data link to transmit a Reply Postponed frame and enter the AWAIT_RESPONSE state */
			return send_segmentAck(0);
		} else 
		{// ConfirmedUnsegmentedReceived
			service_choice = buf[offset++];
			if (0) printf("ConfirmedUnsegmentedReceived\r\n");
			ctx.state = AWAIT_RESPONSE;
			int res = CONF_SERV_ind(npci, service_choice, buf+offset, length-offset); //service_run()
			if (res<0) {
				printf ("ConfirmedUnsegmentedReceived Error=%d\r\n", res);
				if (res==-1) 
					return send_reject(-res);
				else
					return send_error (~res);
					
			} else if (res==0) return send_simpleAck();
			length = res;
			printf ("ConfirmedUnsegmentedReceived len=%d\r\n", length);
			return send_resp(npci->response.buffer+16, length);
		}
		// start RequestTimer
#if 0

		ctx.state = AWAIT_RESPONSE;
        length = service->request(npci, apdu, ctx.seq_pos, &apdu);
		//ctx.state = IDLE;
        if (length==0) {
			return send_simpleAck();
        } else if (length>0) {
			ctx.total_length = length;
			uint8_t max_response=buf[1];
			ctx.MTU = max_response_size(DEVICE_MAX_APDU_LENGTH, max_response);//max_rsponse_size[buf[1] & 0xF];
			// max resp Size of Maximum APDU accepted per Clause 20.1.2.5
			int max_segments_accepted = (max_response>>4)&0x7;
			//max_response_size = (max_response)&0xF;
			if (length>ctx.MTU) {// maximum-transmittable-length с дефрагментацией
				if (DEVICE_SEG_NOT_SUPPORTED || (pdu_type & SA)==0) {// Segmented Response accepted
					return send_abort(SEGMENTATION_NOT_SUPPORTED);
				} else
				if (max_segments_accepted==7 || (ctx.MTU*(1<<max_segments_accepted) > length)) {
					pdu_type = BACnet_ComplexACK_PDU|SEG|MOR|0|0;//|seq_number;
					ctx.seq_number_prev=0;
					ctx.window_size = 1;//ctx.ProposedWindowSize;
					
					length =ctx.MTU;
					
				} else { // CannotSendSegmentedComplexACK
					return send_abort(BUFFER_OVERFLOW);
				}
			} else {// SendUnsegmentedComplexACK
				ctx.state = IDLE;
				return send_resp(BACnet_ComplexACK_PDU, buf, length);
			}
        } else {
			return send_resp(BACnet_Error_PDU, buf, -length);
        }
#endif
    } break;
	case BACnet_SegmentACK_PDU: {
		if (ctx.state==SEGMENTED_RESPONSE) {// подтверждение получения фрагмента SRV (1=server, 0=client) и NAK (negative)
			invoke_id  = buf[1];
			seq_number = buf[2];
			window_size= buf[3];
			window_size= 1;//?
			if (InWindow(seq_number, ctx.seq_number_prev, window_size)) {// подтверждает предыдущий пакет или серию пакетов
				ctx.seq_pos += (seq_number-ctx.seq_number_prev)&SEQ_MASK;
				if (ctx.seq_pos>=ctx.total_length) {// все пакеты отосланы
					ctx.state = IDLE;
					return -1;
				} else {//  FillWindow(InitialSequenceNumber)
					ctx.seq_number_prev = (seq_number+1)&SEQ_MASK;
					return send_seg_resp(apdu_buffer+ctx.seq_pos, ctx.total_length-ctx.seq_pos, window_size);
				}
			} else {// DuplicateACK_Received, restart SegmentTimer
				return -1;//AbortPDU|seq_number;
			}
		} else {// UnexpectedPDU_Received
			return send_abort(INVALID_APDU_IN_THIS_STATE);
		}
	} break;
	case BACnet_Abort_PDU: {
		if ((pdu_type&SRV)==0) ctx.state = IDLE;
		return -1;
	} break;
    case BACnet_Unconfirmed_Request_PDU: {
        service_choice = buf[1];
//		printf("Unconfirmed_Request (%d)\r\n", service_choice);
		UNCONF_SERV_ind(npci, service_choice, buf+2, length-2);
		return -1;// не отсылать ответ
	}
	default:
		return 0;
    }
	return 0;
}


BACnetService_t     device_unconfirmed_services[16] = {
[I_AM] = {I_Am_ind},
[WHO_IS] = {Who_Is_ind},
[TIME_SYNCHRONIZATION] = {TimeSynchronization_ind},
[UNCONFIRMED_TEXT_MESSAGE] = {ConfirmedTextMessage_ind},
[UNCONFIRMED_PRIVATE_TRANSFER]={ConfirmedPrivateTransfer_ind},
[YOU_ARE] = {You_Are_ind},
};

BACnetConfService_t device_confirmed_services[32] = {
[CONFIRMED_TEXT_MESSAGE] = {ConfirmedTextMessage_ind},
// -- File Access Services
[ATOMIC_READ_FILE]  = {AtomicReadFile_ind},
[ATOMIC_WRITE_FILE] = {AtomicWriteFile_ind},
// -- Object Access Services
[ADD_LIST_ELEMENT]  = {AddListElement_ind},
[REMOVE_LIST_ELEMENT] = {RemoveListElement_ind},
[CREATE_OBJECT] = {CreateObject_ind},
[DELETE_OBJECT] = {DeleteObject_ind},
[READ_PROPERTY] = {ReadProperty_ind, ReadProperty_cnf},
[READ_PROPERTY_MULTIPLE] = {ReadPropertyMultiple_ind},
[WRITE_PROPERTY] = {WriteProperty_ind},
[WRITE_PROPERTY_MULTIPLE] = {WritePropertyMultiple_ind},
[READ_RANGE] = {ReadRange_ind},
// -- Remote Device Management Services
[DEVICE_COMMUNICATION_CONTROL] ={},
[CONFIRMED_PRIVATE_TRANSFER] ={ConfirmedPrivateTransfer_ind},
[REINITIALIZE_DEVICE] ={ReinitializeDevice_ind},

};
