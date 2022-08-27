/*! 24 NETWORK SECURITY

The BACnet network security architecture provides device authentication, data hiding, and user authentication. This has been
accomplished within the constraints that BACnet security should allow for:
(a) Application to all BACnet media types (BACnet/IP, MS/TP, etc.)
(b) Application to all BACnet device types (devices, routers, BBMDs)
(c) Application to all message types (broadcast, unicast, confirmed, and unconfirmed)
(d) Application to all message layers (BVLL, network, and application)
(e) Placing non-security-aware devices, if physically secure, behind a security proxy firewall router
(f) Placing secure devices on non-security-aware networks.
*/
enum _SecurityResponseCodes {
success	                =0x00, // General
accessDenied	        =0x01, // Authorization
badDestinationAddress	=0x02, // General
badDestinationDeviceId	=0x03, // General
badSignature	        =0x04, // General
badSourceAddress	    =0x05, // General
badTimestamp	        =0x06, // General
cannotUseKey	        =0x07, // General
cannotVerifyMessageId	=0x08, // General
correctKeyRevision	    =0x09, // General
destinationDeviceIdRequired	=0x0A, // Authorization
duplicateMessage	    =0x0B, // General
encryptionNotConfigured	=0x0C, // General
encryptionRequired	    =0x0D, // Authorization
incorrectKey	        =0x0E, // Authorization
invalidKeyData	        =0x0F, // General
keyUpdateInProgress	    =0x10, // General
malformedMessage	    =0x11, // General
notKeyServer	        =0x12, // General
securityNotConfigured	=0x13, // General
sourceSecurityRequired	=0x14, // Authorization
tooManyKeys	            =0x15, // General
unknownAuthenticationType	=0x16, // Authorization
unknownKey	            =0x17, // General
unknownKeyRevision	    =0x18, // General
unknownSourceMessage	=0x19, // General
};
char* SEC_names[] = {// Table 24-8. Security Response Codes
"X'00' success",
"X'01' accessDenied",// Authorization
"X'02' badDestinationAddress",// General
"X'03' badDestinationDeviceId",// General
"X'04' badSignature",// General
"X'05' badSourceAddress",// General
"X'06' badTimestamp",// General
"X'07' cannotUseKey",// General
"X'08' cannotVerifyMessageId",// General
"X'09' correctKeyRevision",// General
"X'0A' destinationDeviceIdRequired",// Authorization
"X'0B' duplicateMessage",// General
"X'0C' encryptionNotConfigured",// General
"X'0D' encryptionRequired",// Authorization
"X'0E' incorrectKey",// Authorization
"X'0F' invalidKeyData",// General
"X'10' keyUpdateInProgress",// General
"X'11' malformedMessage",// General
"X'12' notKeyServer",// General
"X'13' securityNotConfigured",// General
"X'14' sourceSecurityRequired",// Authorization
"X'15' tooManyKeys",// General
"X'16' unknownAuthenticationType",// Authorization
"X'17' unknownKey",// General
"X'18' unknownKeyRevision",// General
"X'19' unknownSourceMessage",// General
};
int bacnet_network_security()
{
    return 0;
}
