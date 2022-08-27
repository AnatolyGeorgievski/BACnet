/*! \defgroup _14_0 14 FILE ACCESS SERVICES

*/

#include "bacnet.h"
#include "bacnet_asn.h"
#include "bacnet_net.h" // тут живет определение DataUnit_t
#include "bacnet_object.h"

#define SEQUENCE_OF 0x06
/*! \defgroup _14_1 14.1 AtomicReadFile Service

The AtomicReadFile Service is used by a client BACnet-user to perform an open-read-close operation on the contents of the
specified file. The file may be accessed as records or as a stream of octets.
*/
/*! для хранения запросов и откликов используется общая структура данных
НАДО следить чтобоы поля file_record_data и file_data были проинициализированы должным образом
 */
struct _AtomicFile_Request {
    BACnetObjectIdentifier file_identifier;
    uint8_t access_method;
    union {
        struct _stream_access {
            INTEGER file_start_position;
            struct _OctetString file_data;
            //Unsigned requested_octet_count;
        } stream_access;
        struct _record_access {
            INTEGER file_start_record;
            Unsigned record_count;
            BACnetLIST* file_record_data;//
        } record_access;
    };
};
struct _AtomicWriteFile_Ack {
    uint8_t access_method;
    union {
        INTEGER file_start_position;
        INTEGER file_start_record;
    } method_choice;
};
struct _AtomicReadFile_Ack {
    BOOLEAN end_of_file;
    uint8_t access_method;
    union {
        struct _stream_access  stream_access;
        struct _record_access  record_access;
    } method_choice;
};

/*! \defgroup _14_1 14.1 AtomicReadFile Service */
static const ParamSpec_t AtomicReadFile_stream_access[] = {
    {ASN_TYPE_INTEGER,  OFFSET(struct _stream_access, file_start_position)},
    {ASN_TYPE_UNSIGNED, OFFSET(struct _stream_access, file_data.length)},
};
static const ParamSpec_t AtomicReadFile_record_access[] = {
    {ASN_TYPE_INTEGER,  OFFSET(struct _record_access, file_start_record)},
    {ASN_TYPE_UNSIGNED, OFFSET(struct _record_access, record_count)},
//    {SEQUENCE_OF|ASN_TYPE_OCTETS, OFFSET(struct _record_access, file_record_data), OPTIONAL},
};
static const ParamSpec_t AtomicReadFile_method_choice[] = {
[0] = {0, OFFSET(struct _AtomicFile_Request, stream_access), 0,2, AtomicReadFile_stream_access },
[1] = {0, OFFSET(struct _AtomicFile_Request, record_access), 0,2, AtomicReadFile_record_access }
};
static const ParamSpec_t AtomicReadFile_Request_desc[] = {
    {ASN_TYPE_OID,   OFFSET(struct _AtomicFile_Request, file_identifier)},
    {ASN_TYPE_CHOICE,OFFSET(struct _AtomicFile_Request, access_method),0,2, AtomicReadFile_method_choice},
};
int AtomicReadFile_ind(Device_t* device, BACnetValue* node)
{
    struct _AtomicFile_Request request;
    BACnetLIST* list = node->value.list;
    list = bacnet_value_list_parse(list, &request, DEF(AtomicReadFile_Request));

    return 0;
}
static const ParamSpec_t AtomicWriteFile_stream_access[] = {
    {ASN_TYPE_INTEGER,  OFFSET(struct _stream_access, file_start_position)},
    {ASN_TYPE_OCTETS,   OFFSET(struct _stream_access, file_data)},
};
static const ParamSpec_t AtomicWriteFile_record_access[] = {
    {ASN_TYPE_INTEGER,  OFFSET(struct _record_access, file_start_record)},
    {ASN_TYPE_UNSIGNED, OFFSET(struct _record_access, record_count)},
    {SEQUENCE_OF|ASN_TYPE_OCTETS, OFFSET(struct _record_access, file_record_data)},
};
static const ParamSpec_t AtomicReadFile_Ack_method_choice[] = {
[0] = {0, OFFSET(struct _AtomicReadFile_Ack, method_choice.stream_access), 0,2, AtomicWriteFile_stream_access },
[1] = {0, OFFSET(struct _AtomicReadFile_Ack, method_choice.record_access), 0,3, AtomicWriteFile_record_access }
};
static const ParamSpec_t AtomicReadFile_Ack_desc[] = {
    {ASN_TYPE_BOOLEAN,  OFFSET(struct _AtomicReadFile_Ack, end_of_file)},
    {ASN_TYPE_CHOICE,   OFFSET(struct _AtomicReadFile_Ack, access_method),0,2, AtomicReadFile_Ack_method_choice},
};
/*! \brief отклик на запрос о чтении из файла*/
int AtomicReadFile_cnf(Device_t* device, BACnetValue* node)
{
    struct _AtomicReadFile_Ack request;
    BACnetLIST* list = node->value.list;
    list = bacnet_value_list_parse(list, &request, DEF(AtomicReadFile_Ack));

    return 0;
}

int AtomicReadFile_req(BACnetObjectIdentifier file_identifier)
{
    return 0;
}
/*! \defgroup _14_2 14.2 AtomicWriteFile Service */
static const ParamSpec_t AtomicWriteFile_method_choice[] = {
[0] = {0, OFFSET(struct _AtomicFile_Request, stream_access), 0,2, AtomicWriteFile_stream_access },
[1] = {0, OFFSET(struct _AtomicFile_Request, record_access), 0,3, AtomicWriteFile_record_access }
};
static const ParamSpec_t AtomicWriteFile_Request_desc[] = {
    {ASN_TYPE_OID,      OFFSET(struct _AtomicFile_Request, file_identifier)},
    {ASN_TYPE_CHOICE,   OFFSET(struct _AtomicFile_Request, access_method),0,2, AtomicWriteFile_method_choice},
};
static const ParamSpec_t AtomicWriteFile_Ack_choice[] = {
[0] = {ASN_TYPE_INTEGER, OFFSET(struct _AtomicWriteFile_Ack, method_choice.file_start_position)},
[1] = {ASN_TYPE_INTEGER, OFFSET(struct _AtomicWriteFile_Ack, method_choice.file_start_record)}
};
static const ParamSpec_t AtomicWriteFile_Ack_desc[] = {
    {ASN_TYPE_CHOICE,    OFFSET(struct _AtomicWriteFile_Ack, access_method),0,2, AtomicWriteFile_Ack_choice},
};
int AtomicWriteFile_ind(Device_t* device, BACnetValue* node)
{
    struct _AtomicFile_Request request;
    BACnetLIST* list = node->value.list;
    list = bacnet_value_list_parse(list, &request, DEF(AtomicWriteFile_Request));

    return 0;
}

int AtomicWriteFile_cnf(Device_t* device, BACnetValue* node)
{
    struct _AtomicFile_Request request;
    BACnetLIST* list = node->value.list;
    list = bacnet_value_list_parse(list, &request, DEF(AtomicWriteFile_Ack));
// сессия привязывается через идентификатор устройства откуда пришел запрос
// логика принятия решения event_attr_set(event, timestamp, callback, session_data);
// event_queue(event);

    return 0;
}
int AtomicWriteFile_req(Device_t* device, BACnetAddr_t* remote_device_addr,
         struct _AtomicFile_Request * request, DataUnit_t* tr)
{
/*
    struct _AtomicFile_Request request;
    request.file_identifier = *file_identifier;
    request.access_method=0;
    request.method_choice.stream_access.file_start_position = offset;
    request.method_choice.stream_access.file_data.data = data;
    request.method_choice.stream_access.file_data.length = data_len;
*/


    tr->size   = bacnet_paramspec_encode(tr->buffer, request, DEF(AtomicWriteFile_Request));
    ;
{

}
//    device->send_request(remote_device, );

    return 0;
}

#ifdef _WIN32
#include <stdio.h>
extern DeviceInfo_t*  device_address_lookup(Device_t*, uint32_t device_id);
int AtomicWriteFile_test(Device_t* device, uint32_t remote_device_id, uint32_t file_id, char* filename)
{

    FILE * fp = fopen(filename, "rb");
    if (fp == NULL) return -1;

    struct _AtomicFile_Request request;
    request.file_identifier = (BACnetObjectIdentifier){file_id, BACnetObjectType_FILE};
    request.access_method=0;
    DeviceInfo_t* remote_device = device_address_lookup(device, remote_device_id);
    if (remote_device==NULL) {
        fclose(fp);
        return -2;
    }
    DataUnit_t tr;
    uint8_t data[256];
    size_t data_len;
    size_t offset = 0;
    while ((data_len = fread(data,1, sizeof(data),fp))!=0) {
        request.stream_access.file_start_position = offset;
        request.stream_access.file_data.data   = data;
        request.stream_access.file_data.length = data_len;
        AtomicWriteFile_req(device, &remote_device->addr, &request, &tr);
        offset += data_len;
    }
    fclose(fp);
    return 0;
}
#endif
