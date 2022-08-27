#include "bacnet.h"
#include "bacnet_net.h"
#include "bacnet_services.h"
#include "r3_asn.h"
#include <stdio.h>

/*! \defgroup _14_0 14 FILE ACCESS SERVICES
/*! 14.1 AtomicReadFile Service
The AtomicReadFile Service is used by a client BACnet-user to perform an open-read-close operation on the contents of the
specified file. The file may be accessed as records or as a stream of octets.*/

/*! 14.2 AtomicWriteFile Service
The AtomicWriteFile Service is used by a client BACnet-user to perform an open-write-close operation of an OCTET
STRING into a specified position or a list of OCTET STRINGs into a specified group of records in a file. The file may be
accessed as records or as a stream of octets.*/

