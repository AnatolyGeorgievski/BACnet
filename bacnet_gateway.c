/*! BACnet to Modbus Gateway

the gateway provides access to data in the non-BACnet devices through objects within the gateway device.

    \see ANNEX H - COMBINING BACnet NETWORKS WITH NON-BACnet NETWORKS (NORMATIVE)


H.2.1 General Best Practices
H.2.1.1 Caching Writes to Non-BACnet Devices
H.2.1.2 Input, Output, and Value Objects
H.2.1.3 Priority_Array Handling
Gateways are required to implement Priority_Array properties correctly with all 16 entries as defined in Clause 19.2.

H.2.1.4 Handling Requests That Take Too Long
Confirmed requests that cannot be fulfilled within the allowed APDU_Timeout shall result in an abort PDU being returned to the
client. This condition may be caused by requests for too much data or for too many properties that are not cached in the gateway.
One condition when this would occur is when a ReadPropertyMultiple request is received that would require the gateway to
communicate with more non-BACnet devices that it can within the APDU_Timeout because the values are not cached in the
gateway. Under such conditions, the gateway shall return the abort PDU with an abort reason of
APPLICATION_EXCEEDED_REPLY_TIME, and the client is expected to retry the request with fewer properties.

H.5 Using BACnet with EIB/KNX
*/
