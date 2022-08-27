#include "bacnet.h"
#include "bacnet_net.h"
#include "bacnet_services.h"
#include "r3_asn.h"
#include <stdio.h>
static uint32_t decode_oid(uint8_t ** buffer)
{
    uint8_t* buf = *buffer;
    *buffer+=4;
    return __builtin_bswap32(*(uint32_t*)&buf[0]);
}
#define ASSERT(v) if(!(v)) return -1;
/*! \defgroup _15_0 15 OBJECT ACCESS SERVICES
This clause defines application services that collectively provide the means to access and manipulate the properties of
BACnet objects. A BACnet object is any object whose properties are accessible through this protocol regardless of its
particular function within the device in which it resides. These services may be used to access the properties of vendor-
defined objects as well as those of objects specified in this standard.
*/

/*! 15.1 AddListElement Service

The AddListElement service is used by a client BACnet-user to add one or more list elements to an object property that is a
list.
*/
int AddListElement_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    return -1;
}
/*! 15.2 RemoveListElement Service

The RemoveListElement service is used by a client BACnet-user to remove one or more elements from the property of an
object that is a list. If an element is itself a list, the entire element shall be removed. This service does not operate on nested
lists.
*/
int RemoveListElement_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    return -1;
}
/*! 15.3 CreateObject Service

The \b CreateObject service is used by a client BACnet-user to create a new instance of an object. This service may be used to
create instances of both standard and vendor specific objects. The standard object types supported by this service shall be
specified in the PICS. The properties of standard objects created with this service may be initialized in two ways: initial
values may be provided as part of the \b CreateObject service request or values may be written to the newly created object using
the BACnet WriteProperty services. The initialization of non-standard objects is a local matter. The behavior of objects
created by this service that are not supplied, or only partially supplied, with initial property values is dependent upon the
device and is a local matter.
*/
int CreateObject_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    uint8_t *buf = buffer;
    ASSERT(*buf++ == 0xC4);// ASN_TYPE_OID|4
    uint32_t object_identifier = decode_oid(&buf);
    Object_t * object = npci->arg;// можно объект добыть через npci->arg
    if (object->object_identifier == object_identifier) return 0;
    if (npci->peer==NULL) return -1;
    return r3cmd_object_init(npci->peer, object, object_identifier);
//    return r3cmd_object_confirm(&npci->peer->device_objects, object_identifier);
}
int CreateObject_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    DeviceInfo_t *devinfo= npci->local;//->device;
    return r3cmd_object_create(devinfo, buffer, size, npci->response.buffer+16);
}
#if defined(BACNET_MASTER)
int CreateObject_req(BACnetNPCI* npci, uint32_t object_id, const uint16_t *property_id_list, Object_t* object)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    *buf++ = 0x0E;
    if((object_id & OBJECT_ID_MASK)== UNDEFINED) {
        *buf++ = 0x09;
        *buf++ = object_id>>22;
    } else {
        buf = r3_asn_encode_u32(buf, ASN_CONTEXT(1), object_id);
    }
    *buf++ = 0x0F;
    if (object!=NULL && property_id_list!=NULL && property_id_list[0]!=0xFFFF) {// Initial values
        *buf++ = 0x1E;
        const PropertySpec_t * pspec = object->property_list.array;
        uint16_t pspec_length = object->property_list.count;

        int idx;
        for (idx=0; property_id_list[idx]!=0xFFFF; idx++) {
            buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(0), property_id_list[idx]);
            *buf++ = 0x2E;
            buf = r3cmd_property_encode(buf, object, property_id_list[idx]);
            *buf++ = 0x2F;
        }
        *buf++ = 0x1F;
    }
    npci->arg = object;
    return CONF_SERV_req(npci, CREATE_OBJECT, pdu, buf - pdu);
}
#endif
/*! 15.4 DeleteObject Service

The \b DeleteObject service is used by a client BACnet-user to delete an existing object. Although this service is general in the
sense that it can be applied to any object type, it is expected that most objects in a control system cannot be deleted by this
service because they are protected as a security feature. There are some objects, however, that may be created and deleted
dynamically. 'Group' objects and 'Event Enrollment' objects are examples. This service is primarily used to delete objects of
these types but may also be used to remove vendor-specific deletable objects.
*/
int DeleteObject_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    DeviceInfo_t *device = npci->local;//->device;
    return r3cmd_object_delete(device, buffer, size);
}
/*! 15.5 ReadProperty Service

The \b ReadProperty service is used by a client BACnet-user to request the value of one property of one BACnet Object. This
service allows read access to any property of any object, whether a BACnet-defined object or not.
*/
int ReadProperty_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;// ничего не возварщаем на респонзе, ответ на том же буфере
    return r3cmd_read_property(npci->local, buffer, size, npci->response.buffer+16);
}
/*! 15.7 ReadPropertyMultiple Service

The ReadPropertyMultiple service is used by a client BACnet-user to request the values of one or more specified properties
of one or more BACnet Objects. This service allows read access to any property of any object, whether a BACnet-defined
object or not. The user may read a single property of a single object, a list of properties of a single object, or any number of
properties of any number of objects. A 'Read Access Specification' with the property identifier ALL can be used to learn the
implemented properties of an object along with their values.
*/
int ReadPropertyMultiple_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length = 0;
    return r3cmd_read_property_multiple(npci->local, buffer, size, npci->response.buffer+16);
}
/*! 15.8 ReadRange Service

The \b ReadRange service is used by a client BACnet-user to read a specific range of data items representing a subset of data
available within a specified object property. The service may be used with any list or array of lists property.
*/
int ReadRange_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size)
{
    npci->response.length=0;
//    npci->response.length = r3cmd_read_range(&npci->device->tree, buffer, size, npci->response.buffer+16);
    return -1;
}
/*! 15.9 WriteProperty Service

The \b WriteProperty service is used by a client BACnet-user to modify the value of a single specified property of a BACnet
object. This service potentially allows write access to any property of any object, whether a BACnet-defined object or not.
Some implementors may wish to restrict write access to certain properties of certain objects. In such cases, an attempt to
modify a restricted property shall result in the return of an error of 'Error Class' PROPERTY and 'Error Code'
WRITE_ACCESS_DENIED. Note that these restricted properties may be accessible through the use of Virtual Terminal
services or other means at the discretion of the implementor.
*/
int WriteProperty_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size)
{
    npci->response.length=0;
    return r3cmd_write_property(npci->local, buffer, size, 0);
}
/*! 15.10 WritePropertyMultiple Service

The WritePropertyMultiple service is used by a client BACnet-user to modify the value of one or more specified properties
of a BACnet object. This service potentially allows write access to any property of any object, whether a BACnet-defined
object or not.
*/
int WritePropertyMultiple_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size)
{
    npci->response.length=0;
    return r3cmd_write_property_multiple(npci->local, buffer, size, npci->response.buffer+16);
}
#if defined(BACNET_MASTER)
/*! \brief запрсоить значение свойства объекта */
int ReadProperty_req(BACnetNPCI* npci, uint32_t object_id, uint32_t property_identifier, uint16_t array_index)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    buf = r3_asn_encode_u32(buf, ASN_CONTEXT(0), object_id);
    buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), property_identifier);
    if (array_index!=0xFFFF) {
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(2), array_index);
    }
if(0) {
	printf("ReadProperty_req PDU:\r\n");
	int i;
	for(i=0;i<(buf - pdu);i++){
		printf(" %02X", pdu[i]);
	}
	printf("\r\n");
}
    return CONF_SERV_req(npci, READ_PROPERTY, pdu, buf - pdu);
}
/*! \brief запрсоить значение свойства объекта */
int ReadPropertyMultiple_req(BACnetNPCI* npci, uint32_t object_id, const uint16_t* property_id_list)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
    buf = r3_asn_encode_u32(buf, ASN_CONTEXT(0), object_id);
    *buf++ = 0x1E;
    int i;
    for (i=0; property_id_list[i]!=0xFFFF; i++){
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(0), property_id_list[i]);
/*        uint16_t array_index = ~0;
        if (array_index!=0xFFFF) {
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), array_index);
        }
*/
    }
    *buf++ = 0x1F;
    return CONF_SERV_req(npci, READ_PROPERTY_MULTIPLE, pdu, buf - pdu);
}


/*! \brief Отклик на запрос свойства объекта */
int ReadProperty_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
	if (npci->peer==NULL) {// создавать описание устройства
		printf("PEER not exists, create peer\r\n");
		//_Exit(__LINE__);
		DeviceInfo_t *peer = g_slice_new0(DeviceInfo_t);// List_new -- сделать частью списка
		npci->peer=peer;
	}
/*    Device_t* device = npci->peer->device;//network_device(npci);
	if (device==NULL) {
		printf("PEER Device not exists, create peer\r\n");
		return 0;
	}*/
    return r3cmd_write_property(npci->peer, buffer, size, 1);/// создать объект если не существует ???
}
/*! \brief Отклик на запрос списка свойств объекта(ов) */
int ReadPropertyMultiple_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    //Device_t* device = npci->peer->device;//network_device(npci);
    return r3cmd_read_property_multiple_cnf(npci->peer, buffer, size, NULL);// создавать объекты.
}

int WriteProperty_REAL_req(BACnetNPCI * npci, uint32_t object_identifier, uint32_t property_identifier, float value)
{
	uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
//	const PropertySpec_t *pspec = object->instance.property_list.array;
    buf = r3_asn_encode_u32(buf, ASN_CONTEXT(0), object_identifier);
    buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(1), property_identifier);
#if 0
    if (property_array_index!=0xFFFF) {
        buf = r3_asn_encode_unsigned(buf, ASN_CONTEXT(2), property_array_index);
    }
#endif
    *buf++=0x3E;
	buf = r3_asn_encode_u32(buf, ASN_TYPE_REAL, *(uint32_t*)&value);
    *buf++=0x3F;
    printf("%s: Param encode done\r\n", __FUNCTION__);
	return CONF_SERV_req(npci, WRITE_PROPERTY, pdu, buf - pdu);
    //return buf - response_buffer;
}
#endif
