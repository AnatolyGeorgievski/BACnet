#ifndef BACNET_SERVICES_H
#define BACNET_SERVICES_H

#include "bacnet_net.h"

typedef struct _BACnetService BACnetService_t;
struct _BACnetService {
    int (*indication)(BACnetNPCI* npci, uint8_t *buffer, size_t size);//!< представление данных
//    int (*confirm)   (BACnetValue* node);//!< получение отклика
};
typedef struct _BACnetConfService BACnetConfService_t;
struct _BACnetConfService {
    int (*indication)(BACnetNPCI* npci, uint8_t *buffer, size_t size);//!< представление данных
    int (*confirm)   (BACnetNPCI* npci, uint8_t *buffer, size_t size);//!< получение отклика
};

#define WEAK __attribute__ ((weak))

int WEAK AddListElement_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK RemoveListElement_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK CreateObject_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK CreateObject_req(BACnetNPCI* npci, uint32_t object_id, const uint16_t *property_id_list, Object_t* property_value);
int WEAK DeleteObject_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ReadProperty_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ReadProperty_req(BACnetNPCI* npci, uint32_t object_id, uint32_t property_identifier, uint16_t array_index);
int WEAK ReadProperty_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ReadPropertyMultiple_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ReadPropertyMultiple_req(BACnetNPCI* npci, uint32_t object_id, const uint16_t* property_id_list);
int WEAK ReadPropertyMultiple_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ReadRange_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size);
int WEAK WriteProperty_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size);
int WEAK WritePropertyMultiple_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size);
int WEAK WriteProperty_req(BACnetNPCI * npci, ObjectPropertyReference_t* pref, uint8_t* response_buffer);


int WEAK AtomicReadFile_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK AtomicWriteFile_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK AtomicReadFile_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK AtomicWriteFile_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size);

int WEAK ReinitializeDevice_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ReinitializeDevice_req(BACnetNPCI* npci, uint32_t device_oid, uint32_t state, char* passwd);
int WEAK ConfirmedPrivateTransfer_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ConfirmedPrivateTransfer_req(BACnetNPCI *npci, uint32_t service_number, uint8_t *data, size_t data_len);
int WEAK ConfirmedPrivateTransfer_cnf(BACnetNPCI *npci, uint8_t *buffer, size_t length);
int WEAK UnconfirmedPrivateTransfer_ind(BACnetNPCI * npci, uint8_t * buffer, size_t size);
int WEAK UnconfirmedPrivateTransfer_req(BACnetNPCI *npci, uint32_t service_number, uint8_t *data, size_t data_len);
int WEAK I_Am_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK Who_Is_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK Who_Am_I_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK You_Are_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);

int WEAK TimeSynchronization_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ConfirmedTextMessage_ind(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK ConfirmedTextMessage_cnf(BACnetNPCI* npci, uint8_t *buffer, size_t size);
int WEAK I_Am_req(BACnetNPCI* npci);
int WEAK I_Have_req(BACnetNPCI* npci, uint32_t device_oid, uint32_t object_id, char* object_name, size_t name_len);

int WEAK WriteProperty_REAL_req(BACnetNPCI * npci, uint32_t object_identifier, uint32_t property_identifier, float value);

#endif // BACNET_SERVICES_H
