/* J2735 PSM definitions
 * Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef __J2735_PSM_H__
#define __J2735_PSM_H__

#include "j2735_data_component.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PersonalSafetyMessage {
    PersonalDeviceUserType basicType;
    int32_t secMark; /* DSecond (0..65535) */
    int32_t msgCnt; /* MsgCount (0..127) */
    OctetString id; /* TemporaryID (4..4) */
    Position3D position;
    PositionalAccuracy accuracy;
    int32_t speed; /* Velocity (0..8191) */
    int32_t heading; /* Heading (0..28800) */
    BOOL accelSet_option;
    AccelerationSet4Way accelSet;
    BOOL pathHistory_option;
    PathHistory pathHistory;
    BOOL pathPrediction_option;
    PathPrediction pathPrediction;
    BOOL propulsion_option;
    PropelledInformation propulsion;
    BOOL useState_option;
    BitString useState; /* PersonalDeviceUsageState (9..9) */
    BOOL crossRequest_option;
    BOOL crossRequest; /* PersonalCrossingRequest */
    BOOL crossState_option;
    BOOL crossState; /* PersonalCrossingInProgress */
    BOOL clusterSize_option;
    NumberOfParticipantsInCluster clusterSize;
    BOOL clusterRadius_option;
    int32_t clusterRadius; /* PersonalClusterRadius (0..100) */
    BOOL eventResponderType_option;
    PublicSafetyEventResponderWorkerType eventResponderType;
    BOOL activityType_option;
    BitString activityType; /* PublicSafetyAndRoadWorkerActivity (6..6) */
    BOOL activitySubType_option;
    BitString activitySubType; /* PublicSafetyDirectingTrafficSubType (7..7) */
    BOOL assistType_option;
    BitString assistType; /* PersonalAssistive (6..6) */
    BOOL sizing_option;
    BitString sizing; /* UserSizeAndBehaviour (5..5) */
    BOOL attachment_option;
    Attachment attachment;
    BOOL attachmentRadius_option;
    int32_t attachmentRadius; /* AttachmentRadius (0..200) */
    BOOL animalType_option;
    AnimalType animalType;
    BOOL regional_option;
    Reg_PersonalSafetyMessageList regional;
} PersonalSafetyMessage;

#ifdef __cplusplus
}
#endif

#endif /* __J2735_PSM_H__ */
