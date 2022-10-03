/* J2735 EVA definitions
 * Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef __J2735_EVA_H__
#define __J2735_EVA_H__

#include "j2735_data_component.h"
#include "j2735_rsa.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmergencyVehicleAlert {
    BOOL timeStamp_option;
    int32_t timeStamp; /* MinuteOfTheYear (0..527040) */
    BOOL id_option;
    OctetString id; /* TemporaryID (4..4) */
    RoadSideAlert rsaMsg;
    BOOL responseType_option;
    ResponseType responseType;
    BOOL details_option;
    EmergencyDetails details;
    BOOL mass_option;
    int32_t mass; /* VehicleMass (0..255) */
    BOOL basicType_option;
    VehicleType basicType;
    BOOL vehicleType_option;
    VehicleGroupAffected vehicleType;
    BOOL responseEquip_option;
    IncidentResponseEquipment responseEquip;
    BOOL responderType_option;
    ResponderGroupAffected responderType;
    BOOL regional_option;
    Reg_EmergencyVehicleAlertList regional;
} EmergencyVehicleAlert;

#ifdef __cplusplus
}
#endif

#endif /* __J2735_EVA_H__ */
