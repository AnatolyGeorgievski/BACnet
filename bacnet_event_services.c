#include "bacnet.h"
#include "bacnet_net.h"
#include "bacnet_services.h"
#include "r3_asn.h"
#include <stdio.h>

/*! \defgroup _13_0 13 ALARM AND EVENT SERVICES 
*/
/*! 13.7 UnconfirmedCOVNotification Service
The UnconfirmedCOVNotification Service is used to notify subscribers about changes that may have occurred to the
properties of a particular object, or to distribute object properties of wide interest (such as outside air conditions) to many
devices simultaneously without a subscription. Subscriptions for COV notifications are made using the SubscribeCOV
service. For unsubscribed notifications, the algorithm for determining when to issue this service is a local matter and may be
based on a change of value, periodic updating, or some other criteria.*/
int UnconfirmedCOVNotification_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    return -1;
}
int UnconfirmedCOVNotification_req(BACnetNPCI* npci, uint32_t subscriber_process_identifier/* ,... */)
{
    npci->response.length=0;
    return -1;
}

/*! 13.14 SubscribeCOV Service
The SubscribeCOV service is used by a COV-client to subscribe for the receipt of notifications of changes that may occur to
the properties of a particular object. Certain BACnet standard objects may optionally support COV reporting. If a standard
object provides COV reporting, then changes of value of specific properties of the object, in some cases based on
programmable increments, trigger COV notifications to be sent to one or more subscriber clients. Typically, COV
notifications are sent to supervisory programs in BACnet client devices or to operators or logging devices. Proprietary objects
may support COV reporting at the implementor's option. */
int SubscribeCOV_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    return -1;
}


// subscribe-cov-property (38), -- Alarm and Event Service
/*! 13.15 SubscribeCOVProperty Service
The \b SubscribeCOVProperty service is used by a COV-client to subscribe for the receipt of notifications of changes that may
occur to the properties of a particular object. Any object may optionally support COV reporting. If a standard object provides
COV reporting, then changes of value of subscribed-to properties of the object, in some cases based on programmable
increments, trigger COV notifications to be sent to one or more subscriber clients. Typically, COV notifications are sent to
supervisory programs in BACnet client devices or to operators or logging devices. */
int SubscribeCOVProperty_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size)
{
    npci->response.length=0;
    return -1;
}


int SubscribeCOVProperty_req(BACnetNPCI* npci, ObjectIdentifier_t object_identifier, enum _BACnetPropertyIdentifier property_identifier , uint32_t subscriber_id)
{
    uint8_t * pdu = npci->response.buffer+16;
    uint8_t * buf = pdu;
	buf = r3_asn_encode_u32(buf, ASN_CONTEXT(0), object_identifier);
	UNCONF_SERV_req(npci, SUBSCRIBE_COV, pdu, buf - pdu);
    return -1;
}
int SubscribeCOV_req(BACnetNPCI* npci, ObjectIdentifier_t object_identifier)
{
    npci->response.length=0;
    return -1;
}

