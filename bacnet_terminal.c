/*! 17 VIRTUAL TERMINAL SERVICES
*/
#include "bacnet.h"
#include "bacnet_object.h"
enum _BACnetVTClass {
	DEFAULT_TERMINAL,
	ANSI_X3_64,
	DEC_VT52,
	DEC_VT100,
	DEC_VT220,
	HP_700_94,
	IBM_3130,
};

/*! 17.2 VT-Open Service 
The VT-Open service is used to establish a VT-session with a peer VT-user. The service request includes a VT-class type that
identifies a particular set of assumptions about the character repertoire and encoding to be used with this session
*/
int VT_Open_ind(Device_t *device, BACnetValue* node)
{
	
	return 0;
}
/*! 17.3 VT-Close Service 
The VT-Close service is used to terminate a previously established VT-session with a peer VT-user. The service request may
specify a particular VT-session to be terminated or a list of VT-sessions to be terminated

	17.3.2 Service Procedure
After verifying the validity of the request, the responding BACnet-user shall attempt to terminate each VT-session specified
by the 'List of Remote VT Session Identifiers' parameter. From the viewpoint of the responding BACnet-user, these are
'Local VT Session Identifiers'. If one or more of the specified VT-sessions cannot be terminated for some reason, then all of
the specified sessions that can be terminated shall be terminated and a 'Result (-)' response shall be returned. If all of the
specified VT-sessions are successfully terminated, then the 'Result (+)' response shall be returned.
Copyrighted material licensed to Anatoly Georgievskii on 2017-06-14 for licensee's use only. All rights reserved. No further reproduction or distribution is permitted. Distributed by Techstreet for ASHRAE, www.te
*/
int VT_Close_ind(Device_t *device, BACnetValue* node)
{
	
	return 0;
}
/*! 17.4 VT-Data Service 
The VT-Data service is used to exchange data with a peer VT-user through a previously established VT-session. The sending
BACnet-user provides new input for the peer VT-user, which may accept or reject the new data. If the new data are rejected,
then it is up to the sending BACnet-user to retry the request at a later time.

*/
int VT_Data_ind(Device_t *device, BACnetValue* node)
{
	
	return 0;
}
int VT_Data_req(Device_t *device, BACnetValue* node)
{
	
	return 0;
}
