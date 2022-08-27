/*! \defgroup _16_0 16 REMOTE DEVICE MANAGEMENT SERVICES

    \{
*/


#include "cmsis_os.h"
#include "r3_asn.h"
#include "bacnet.h"
#include "bacnet_net.h"
#include "bacnet_error.h"
#include "bacnet_services.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


#define ASSERT(v) if(!(v)) return osErrorParameter;
static inline uint32_t decode_u32(uint8_t *buf)
{
    return __builtin_bswap32(*(uint32_t*)&buf[0]);
}
static inline uint16_t decode_u16(uint8_t *buf)
{
    return __builtin_bswap16(*(uint16_t*)&buf[0]);
}
static uint32_t decode_unsigned(uint8_t *buf, int size)
{
    switch(size) {
    case 4: return __builtin_bswap32(*(uint32_t*)&buf[0]);
    case 3: return __builtin_bswap32((*(uint32_t*)&buf[0])<<8);
    case 2: return __builtin_bswap16(*(uint16_t*)&buf[0]);
    case 1: return buf[0];
    default: return 0;
    }
}
extern int WEAK R3_cmd_snmp_recv2(uint8_t code, uint8_t * buffer, int *length);


/*! 16.1 DeviceCommunicationControl Service

The \b DeviceCommunicationControl service is used by a client BACnet-user to instruct a remote device to stop initiating and
optionally stop responding to all APDUs (except DeviceCommunicationControl or, if supported, ReinitializeDevice) on the
communication network or internetwork for a specified duration of time. This service is primarily used by a human operator
for diagnostic purposes. A password may be required from the client BACnet-user prior to executing the service. The time
duration can be set to "indefinite," meaning communication must be re-enabled by a \b DeviceCommunicationControl or, if
supported, ReinitializeDevice service, not by time.
*/

/*! 16.2 ConfirmedPrivateTransfer Service

The \b ConfirmedPrivateTransfer is used by a client BACnet-user to invoke proprietary or non-standard services in a remote
device. The specific proprietary services that may be provided by a given device are not defined by this standard. The
PrivateTransfer services provide a mechanism for specifying a particular proprietary service in a standardized manner. The
only required parameters for these services are a vendor identification code and a service number. Additional parameters
may be supplied for each service if required. The form and content of these additional parameters, if any, are not defined by
this standard. The vendor identification code and service number together serve to unambiguously identify the intended
purpose of the information conveyed by the remainder of the APDU or the service to be performed by the remote device
based on parameters in the remainder of the APDU.*/
int ConfirmedPrivateTransfer_ind(BACnetNPCI* npci, uint8_t *pdu, size_t length)
{
    printf("ConfirmedPrivateTransfer_ind\n");
	npci->response.length=0;
    uint8_t* buf = pdu;
    ASSERT(*buf++ == 0x0A);
    uint16_t vendor_id = decode_u16(buf); buf+=2;
    ASSERT(vendor_id == 0x9148);
    ASSERT((buf[0]&0xF8) == 0x18);
    int size = (*buf++)&0x7;
    uint32_t service_number = decode_unsigned(buf,size); buf+=size;
    size_t data_len=0;
    uint16_t node_tag;
    if ((buf-pdu)<length && *buf == 0x2E) {
        buf++;
        buf = r3_decode_tag(buf, &node_tag, &data_len);
        ASSERT(buf-pdu == length-data_len-1);
        ASSERT(buf[data_len] == 0x2F);
    }
    int resp_len = data_len;
    uint8_t   resp_code = R3_cmd_snmp_recv2(service_number, buf, &resp_len); /* Обработка команды R3-SNMP */
	uint8_t * resp = buf;
	// если resp_code==OK
	 buf = pdu = npci->response.buffer+16;
    *buf++ = 0x0A;
    *buf++ = vendor_id>>8;
    *buf++ = vendor_id & 0xFF;
    *buf++ = 0x19;
    *buf++ = service_number;
//    buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), service_number);
    if (resp_len) {
        *buf++ = 0x2E;
        *buf++ = ASN_TYPE_OCTETS| 0x5;
        *buf++ = resp_len & 0xFF;
        //buf = r3_encode_tag(buf, tag, data_len);
        __builtin_memcpy(buf, resp, resp_len); buf+=resp_len;
        *buf++ = 0x2F;
    }
    return 0;
}

/*! 16.3 UnconfirmedPrivateTransfer Service

The \b UnconfirmedPrivateTransfer is used by a client BACnet-user to invoke proprietary or non-standard services in a
remote device. The specific proprietary services that may be provided by a given device are not defined by this standard.
The PrivateTransfer services provide a mechanism for specifying a particular proprietary service in a standardized manner.
The only required parameters for these services are a vendor identification code and a service number. Additional
parameters may be supplied for each service if required. The form and content of these additional parameters, if any, are not
defined by this standard. The vendor identification code and service number together serve to unambiguously identify the
intended purpose of the information conveyed by the remainder of the APDU or the service to be performed by the remote
device based on parameters in the remainder of the APDU.*/
extern DeviceInfo_t* ThisDevice;
extern int service_run;
enum _ReinitializeDeviceState {
    COLDSTART       =(0),
    WARMSTART       =(1),
    START_BACKUP    =(2),
    END_BACKUP      =(3),
    START_RESTORE   =(4),
    END_RESTORE     =(5),
    ABORT_RESTORE   =(6),
    ACTIVATE_CHANGES=(7)
};

/*! 16.4 ReinitializeDevice Service

The \b ReinitializeDevice service is used by a client BACnet-user to instruct a remote device to reboot itself (cold start), reset
itself to some predefined initial state (warm start), to activate network port object changes, or to control the backup or
restore procedure. Resetting or rebooting a device is primarily initiated by a human operator for diagnostic purposes. Use of
this service during the backup or restore procedure is usually initiated on behalf of the user by the device controlling the
backup or restore. Due to the sensitive nature of this service, a password may be required by the responding BACnet-user
prior to executing the service.*/

int ReinitializeDevice_ind(BACnetNPCI* npci, uint8_t *pdu, size_t length)
{
    printf("ReinitializeDevice_ind\n");
    uint8_t* buf = pdu;
    ASSERT(*buf++ == 0x09);
	uint8_t reinitialized_state=*buf++;
	Device_t* device = ThisDevice->device;
//	if () {// password
	switch (reinitialized_state) {
	case ACTIVATE_CHANGES:
		// применить настройку адресов
		if (device->system_status == OPERATIONAL) {
			device->system_status = OPERATIONAL_READ_ONLY;
			// запустить службу сохранения настроек
			//r3cmd_store();
		}
		break;
	case START_BACKUP: 
		device->backup_and_restore_state = PREPARING_FOR_BACKUP;// после выполнения подготовки ставим PERFORMING_A_BACKUP или BACKUP_FAILURE
		device->system_status = BACKUP_IN_PROGRESS;
		break;
	case END_BACKUP:
		device->backup_and_restore_state = BACKUP_STATE_IDLE;
		device->system_status = OPERATIONAL;
		break;
	case START_RESTORE:
		device->backup_and_restore_state = PREPARING_FOR_RESTORE;// после выполнения подготовки ставим PERFORMING_A_RESTORE или RESTORE_FAILURE
		device->system_status = DOWNLOAD_IN_PROGRESS;
		break;
	case END_RESTORE:
		if (device->backup_and_restore_state != RESTORE_FAILURE)
		{ // If the restore is successful
			device->last_restore_time = (DateTime_t){device->local_date, device->local_time};
		}
	case ABORT_RESTORE:
		device->backup_and_restore_state = BACKUP_STATE_IDLE;
		device->system_status = OPERATIONAL;
		break;
	case COLDSTART:
	case WARMSTART:
	// есть вариант заглушить все и переписать флеш
#if !defined(__linux__) && !defined(_WIN32)
		NVIC_SystemReset();
#else
		service_run = 0;
#endif
		break;
	default:
		// вернуть сообщени об ошибке??
		break;
	}
	return 0;
}

/*! 16.5 ConfirmedTextMessage Service

The \b ConfirmedTextMessage service is used by a client BACnet-user to send a text message to another BACnet device. This
service is not a broadcast or multicast service. This service may be used in cases when confirmation that the text message
was received is required. The confirmation does not guarantee that a human operator has seen the message. Messages may
be prioritized into normal or urgent categories. In addition, a given text message may be optionally classified by a numeric
class code or class identification string. This classification may be used by the receiving BACnet device to determine how
to handle the text message. For example, the message class might indicate a particular output device on which to print text
or a set of actions to take when the text is received. In any case, the interpretation of the class is a local matter.*/

/*! 16.6 UnconfirmedTextMessage Service

The \b UnconfirmedTextMessage service is used by a client BACnet-user to send a text message to one or more BACnet
devices. This service may be broadcast, multicast, or addressed to a single recipient. This service may be used in cases
where confirmation that the text message was received is not required. Messages may be prioritized into normal or urgent
categories. In addition, a given text message may optionally be classified by a numeric class code or class identification
string. This classification may be used by receiving BACnet devices to determine how to handle the text message. For
example, the message class might indicate a particular output device on which to print text or a set of actions to take when
the text message is received. In any case, the interpretation of the class is a local matter.*/


/*! 16.7 TimeSynchronization Service

The \b TimeSynchronization service is used by a requesting BACnet-user to notify a remote device of the correct current time.
This service may be broadcast, multicast, or addressed to a single recipient. Its purpose is to notify recipients of the correct
current time so that devices may synchronize their internal clocks with one another.*/
#undef  ASSERT
#define ASSERT(v) if(!(v)) return osErrorParameter;
int TimeSynchronization_ind(BACnetNPCI* npci, uint8_t *pdu, size_t size)
{
        struct tm tm;
        struct tm *t=&tm;
        ASSERT(pdu[0] == 0xA4);
        t->tm_year = pdu[1];
        t->tm_mon  = pdu[2]-1;
        t->tm_mday = pdu[3];
        t->tm_wday = pdu[4];
        if (t->tm_wday==7) t->tm_wday=0;
        ASSERT(pdu[5] == 0xB4);
        t->tm_hour = pdu[6];
        t->tm_min  = pdu[7];
        t->tm_sec  = pdu[8];
        //pdu[9]= 0;
        printf("DateTime: %02d-%02d-%04d\n", t->tm_mday, t->tm_mon+1, t->tm_year+1900 );
		// Update Local_Time and Local_Date properties of the Device object
		Device_t* device = npci->local->device;
		device->local_time = 0;
		device->local_date = 0;
		// RTC update?
	return 0;
}
/*! 16.9 Who-Has and I-Have Services

The \b Who-Has service is used by a sending BACnet-user to identify the Device object identifiers and network addresses of
other BACnet devices whose local databases contain an object with a given Object_Name or a given Object_Identifier. The IHave service is used to respond to Who-Has service requests or to advertise the existence of an object with a given
Object_Name or Object_Identifier. The \b I-Have service request may be issued at any time and does not need to be preceded
by the receipt of a \b Who-Has service request. The \b Who-Has and \b I-Have services are unconfirmed services. */


/*! 16.10 Who-Is and I-Am Services

The \b Who-Is service is used by a sending BACnet-user to determine the Device object identifier, the network address, or both,
of other BACnet devices that share the same internetwork. The Who-Is service is an unconfirmed service. The Who-Is
service may be used to determine the Device object identifier and network addresses of all devices on the network, or to
determine the network address of a specific device whose Device object identifier is known, but whose address is not. The IAm service is also an unconfirmed service. The I-Am service is used to respond to Who-Is service requests. However, the IAm service request may be issued at any time. It does not need to be preceded by the receipt of a Who-Is service request. In
particular, a device may wish to broadcast an I-Am service request when it powers up. The network address is derived either
from the MAC address associated with the \b I-Am service request, if the device issuing the request is on the local network, or
from the NPCI if the device is on a remote network.

16.10.2 Service Procedure
The sending BACnet-user shall transmit the Who-Is unconfirmed request, normally using a broadcast address. If the 'Device
Instance Range Low Limit' and 'Device Instance Range High Limit' parameters are omitted, then all receiving BACnet-users
shall return their Device Object_Identifier in individual responses using the I-Am service. If the 'Device Instance Range Low
Limit' and 'Device Instance Range High Limit' parameters are present, then only those receiving BACnet-users whose Device
Object_Identifier instance number falls within the range greater than or equal to 'Device Instance Range Low Limit' and less
than or equal to 'Device Instance Range High Limit' shall return their Device Object_Identifier using the I-Am service.
If the receiving BACnet-user has a Slave_Proxy_Enable property and the Slave_Proxy_Enable for the receiving port is
TRUE, then the BACnet-user shall respond with an I-Am unconfirmed request for each of the slave devices on the MS/TP
network that are present in the Slave_Address_Binding property and that match the device range parameters. The I-Am
unconfirmed requests that are generated shall be generated as if the slave device originated the service. If the I-Am
unconfirmed request is to be placed onto the MS/TP network on which the slave device resides, then the MAC address
included in the packet shall be that of the slave device. In the case where the I-Am unconfirmed request is to be placed onto a
network other than that on which the slave device resides, then the network layer shall contain SLEN and SNET filled in with
the slave device's MAC address as if it were routing a packet originally generated by the slave device.*/

#if defined(BACNET_MASTER)

/*! \brief запускается по случаю обнаружения нового устройства в сети
    уведомление отсылается только в сеть типа IPv4
 */
void I_Have_ind(BACnetNPCI* npci, uint8_t * data, size_t data_len)
{

}

//uint8_t broadcast_response[48];
/*! \brief
 */
int I_Am_req(BACnetNPCI* npci)
{
    if (npci->response.buffer==NULL) {
        return -1;//_Exit(37);
    }
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    Device_t* device = npci->local->device;
    buf = r3_asn_encode_u32     (buf, ASN_TYPE_OID,        device->instance.object_identifier);
    buf = r3_asn_encode_unsigned(buf, ASN_TYPE_UNSIGNED,   device->max_apdu_length_accepted);
    buf = r3_asn_encode_unsigned(buf, ASN_TYPE_ENUMERATED, device->segmentation_supported);
    buf = r3_asn_encode_unsigned(buf, ASN_TYPE_UNSIGNED,   device->vendor_identifier);
    UNCONF_SERV_req(npci, I_AM, pdu, buf - pdu);
    return 0;
}
/*! \brief Запрос Who-Is - запрашивает перекличку устройств
*/
int Who_Is_req(BACnetNPCI* npci, uint32_t device_instance_range_low_limit, uint32_t device_instance_range_high_limit)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    if (device_instance_range_low_limit!=0)
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(0), device_instance_range_low_limit);
    if (device_instance_range_high_limit!=0x3FFFFF)
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), device_instance_range_high_limit);
    UNCONF_SERV_req(npci, WHO_IS, pdu, buf - pdu);
    return 0;
}
int Who_Is_ind(BACnetNPCI* npci, uint8_t * data, size_t data_len)
{
    uint8_t * buf = data;
    int size;
    uint32_t device_instance_range_low_limit = 0;
    uint32_t device_instance_range_high_limit = 0x3FFFFF;

    if((buf[0] & 0xF8) == ASN_CONTEXT(0)){// optional
        size =(*buf++) & 0x07;
        device_instance_range_low_limit = decode_unsigned(buf, size); buf+=size;
    }
//printf("Who_Is_ind:2\n");
    if((buf[0] & 0xF8) == ASN_CONTEXT(1)){// optional
        size =(*buf++) & 0x07;
        device_instance_range_high_limit = decode_unsigned(buf, size); buf+=size;
    }
if (npci==NULL) printf("Who_Is_req:npci is NULL\n");
if (npci->local->device==NULL) printf("Who_Is_req:device is NULL\n");
    Device_t *device = npci->local->device;
    uint32_t device_instance = device->instance.object_identifier & 0x3FFFFF;// 22 бита
    if (device_instance_range_low_limit<=device_instance &&  device_instance <= device_instance_range_high_limit) {

        printf("Who-Is ind, send I am request\n");
        npci->DA = npci->SA; npci->control &= ~0x08; /// Bit 3: Source specifier
        I_Am_req(npci);
    } else {
        printf("device_instance =%d\n", device_instance);
    }
    return 0;
}

#undef  ASSERT
#define ASSERT(v) if(!(v)) return osErrorParameter;
int ConfirmedPrivateTransfer_req(BACnetNPCI *npci, uint32_t service_number, uint8_t *data, size_t data_len/*, uint16_t offset*/)
{
    const uint16_t vendor_id = 0x9148;
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    *buf++ = 0x0A;
    *buf++ = vendor_id>>8;
    *buf++ = vendor_id & 0xFF;
    *buf++ = 0x19;
    *buf++ = service_number;
//    buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), service_number);
    if (data_len) {
        *buf++ = 0x2E;
        *buf++ = ASN_TYPE_OCTETS| 0x5;
        *buf++ = data_len;
        //buf = r3_encode_tag(buf, tag, data_len);
        __builtin_memcpy(buf, data, data_len); buf+=data_len;
        *buf++ = 0x2F;
    }
    return CONF_SERV_req(npci, CONFIRMED_PRIVATE_TRANSFER, pdu, buf - pdu);
}

int UnconfirmedPrivateTransfer_req(BACnetNPCI *npci, uint32_t service_number, uint8_t *data, size_t data_len)
{
    const uint16_t vendor_id = 0x9148;
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    *buf++ = 0x0A;
    *buf++ = vendor_id>>8;
    *buf++ = vendor_id & 0xFF;
    *buf++ = 0x19;
    *buf++ = service_number;
//    buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), service_number);
    if (data_len) {
        *buf++ = 0x2E;
        *buf++ = ASN_TYPE_OCTETS| 0x5;
        *buf++ = data_len;
        //buf = r3_encode_tag(buf, tag, data_len);
        __builtin_memcpy(buf, data, data_len); buf+=data_len;
        *buf++ = 0x2F;
    }
    UNCONF_SERV_req(npci, UNCONFIRMED_PRIVATE_TRANSFER, pdu, buf - pdu);
	return 0;
}

int ConfirmedPrivateTransfer_cnf(BACnetNPCI *npci, uint8_t *buffer, size_t length)
{
    uint8_t * buf = buffer;
    ASSERT(*buf++ == 0x0A);
    uint16_t vendor_id = decode_u16(buf); buf+=2;
    ASSERT(vendor_id == 0x9148);
    ASSERT(*buf++ == 0x19);
    uint32_t service_number = *buf++;//decode_unsigned(buf, size); buf+=size;
    size_t node_len = 0;
    if (length > (buf - buffer)) {
        ASSERT(*buf++ == 0x2E);
        //uint16_t tag;
        ASSERT(*buf++ == (ASN_TYPE_OCTETS|0x5));
        node_len = *buf++;
        //buf = r3_decode_tag(buf, &tag, &node_len);
        //__builtin_memcpy(buf, data, data_len); buf+=data_len;
        ASSERT(buf-buffer+node_len+1  == length);
        ASSERT(buf[node_len] == 0x2F);
    }
    ///Передать управление и ссылку на буфер
    return 0;
}

/*!
coldstart (0),
warmstart (1),
start-backup (2),
end-backup (3),
start-restore (4),
end-restore (5),
abort-restore (6),
activate-changes (7)
*/
int ReinitializeDevice_req(BACnetNPCI* npci, uint32_t device_oid, uint32_t state, char* passwd)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    //buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(0), state&0x7);
	*buf++ =  ASN_CONTEXT(0)|1;
	*buf++ = state & 0x07;
    if (passwd!=NULL) {
/// \todo доделать пароль
    }
    return CONF_SERV_req(npci, REINITIALIZE_DEVICE, pdu, buf - pdu);
}

/*!
You-Are-Request ::= SEQUENCE {
    vendorID Unsigned,
    modelName CharacterString,
    serialNumber CharacterString,
    deviceIdentifier BACnetObjectIdentifier OPTIONAL,
    deviceMACAddress OctetString OPTIONAL
}
*/
int You_Are_req (BACnetNPCI* npci, DeviceInfo_t *peer)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    Device_t* device = peer->device;
    buf = r3_asn_encode_unsigned(buf, ASN_TYPE_UNSIGNED, device->vendor_identifier);
    buf = r3_asn_encode_string(buf, ASN_TYPE_STRING, device->model_name, strlen(device->model_name));
    buf = r3_asn_encode_string(buf, ASN_TYPE_STRING, device->serial_number, strlen(device->model_name));
    if ((device->instance.object_identifier & OBJECT_ID_MASK)!= UNDEFINED)
        buf = r3_asn_encode_u32(buf, ASN_TYPE_OID, device->instance.object_identifier);
    if (peer->addr.len!=0)
        buf = r3_asn_encode_octets(buf, ASN_TYPE_OCTETS, peer->addr.mac, peer->addr.len);/// ошибка? адрес включает еще что-то? IPv4+port
    UNCONF_SERV_req(npci, YOU_ARE, pdu, buf - pdu);
    return 0;
}
/*! \brief Запросить конфигурацию устройства,

    Используется для получения конфигурации через запрос Who-Am-I,
    если не назначен device_identifier или mac_address
 */
int Who_Am_I_req (BACnetNPCI* npci, DeviceInfo_t* devinfo)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    Device_t* device = devinfo->device;
    buf = r3_asn_encode_unsigned(buf, ASN_TYPE_UNSIGNED, device->vendor_identifier);
    buf = r3_asn_encode_string(buf, ASN_TYPE_STRING, device->model_name, strlen(device->model_name));
    buf = r3_asn_encode_string(buf, ASN_TYPE_STRING, device->serial_number, strlen(device->model_name));
    UNCONF_SERV_req(npci, WHO_AM_I, pdu, buf - pdu);
    return 0;
}

int I_Have_req(BACnetNPCI* npci, uint32_t device_oid, uint32_t object_id, char* object_name, size_t name_len)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    buf = r3_asn_encode_u32(buf, ASN_TYPE_OID, device_oid);
    buf = r3_asn_encode_u32(buf, ASN_TYPE_OID, object_id);
    buf = r3_asn_encode_string(buf, ASN_TYPE_OID, object_name, name_len);
    UNCONF_SERV_req(npci, I_HAVE, pdu, buf - pdu);
    return 0;
}
#endif
extern DeviceInfo_t* ThisDevice;
/*! \brief Обработка запроса You-Are
    \see [DM_DDA_B] Device Management -- Dynamic Device Assignment-B

 */
int You_Are_ind (BACnetNPCI* npci, uint8_t *data, size_t data_len)
{
	printf(" = You_Are_ind\r\n");
    uint8_t * buf = data;
    int size;
    ASSERT((buf[0] & 0xF8) == ASN_TYPE_UNSIGNED);
    size =(*buf++) & 0x07;
    uint16_t vendor_identifier = decode_unsigned(buf, size); buf+=size;

    ASSERT((buf[0] & 0xF8) == ASN_TYPE_STRING);
    size =(*buf++) & 0x07;
    if (size==5) size = *buf++;
    int model_name_len = size-1;
    char* model_name = (char*)buf+1; buf+=size;

    ASSERT((buf[0] & 0xF8) == ASN_TYPE_STRING);
    size =(*buf++) & 0x07;
    if (size==5) size = *buf++;
    int serial_number_len = size-1;
    char* serial_number = (char*)buf+1; buf+=size;
    uint32_t device_identifier = OID(_DEVICE, UNDEFINED);
    if((buf[0] & 0xF8) == ASN_TYPE_OID) {
        size =(*buf++) & 0x07;
        device_identifier = decode_u32(buf); buf+=size;
    }
    uint8_t *mac_address = NULL;
    if((buf[0] & 0xF8) == ASN_TYPE_OCTETS) {
        size =(*buf++) & 0x07;
        if (size==5) size = *buf++;
        mac_address = buf;
        buf+=size;
    }

    Device_t* device = ThisDevice->device;// npci->local
    if((device->instance.object_identifier&OBJECT_ID_MASK)==UNDEFINED
    &&  device->model_name!=NULL
    &&  device->serial_number!=NULL
    &&  strncmp(device->model_name, model_name, model_name_len)==0
    &&  strncmp(device->serial_number, serial_number, serial_number_len)==0) {
        if (device_identifier!=DEFAULT_DEVICE && device->instance.object_identifier==DEFAULT_DEVICE) {
            //device->instance.object_identifier = device_identifier;
			printf(" == update device_id=%X\r\n", device_identifier);
            if (device_identifier!= device->instance.object_identifier) {
                device->instance.object_identifier = device_identifier;
                if(device->instance.object_identifier == DEFAULT_DEVICE) {
                    r3cmd_object_init(ThisDevice, (Object_t*)ThisDevice->device, device_identifier);
                }
            }
        }
        if (mac_address!=NULL) {
            memcpy(ThisDevice->addr.mac, mac_address, size);
            // мак адрес можно менять для MSTP после использования команды ReinitializeDevice_req
        }
    }
    return 0;
}


//! \}
