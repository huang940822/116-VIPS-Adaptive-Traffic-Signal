/* J2735 ICA definitions
 * Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef __J2735_ICA_H__
#define __J2735_ICA_H__

#include "j2735_data_component.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IntersectionCollision {
    int32_t msgCnt; /* MsgCount (0..127) */
    OctetString id; /* TemporaryID (4..4) */
    BOOL timeStamp_option;
    int32_t timeStamp; /* MinuteOfTheYear (0..527040) */
    BOOL partOne_option;
    BSMcoreData partOne;
    BOOL path_option;
    PathHistory path;
    BOOL pathPrediction_option;
    PathPrediction pathPrediction;
    IntersectionReferenceID intersectionID;
    ApproachOrLane laneNumber;
    BitString eventFlag; /* VehicleEventFlags (13..13) */
    BOOL regional_option;
    Reg_IntersectionCollisionList regional;
} IntersectionCollision;

#ifdef __cplusplus
}
#endif

#endif /* __J2735_ICA_H__ */
