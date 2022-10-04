/* J2735 RSA definitions
 * Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef __J2735_RSA_H__
#define __J2735_RSA_H__

#include "j2735_data_component.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RoadSideAlert {
    int32_t msgCnt; /* MsgCount (0..127) */
    BOOL timeStamp_option;
    int32_t timeStamp; /* MinuteOfTheYear (0..527040) */
    int32_t typeEvent; /* ITIScodes (0..65535) */
    BOOL description_option;
    ITIScodesList description;
    BOOL priority_option;
    OctetString priority; /* Priority (1..1) */
    BOOL heading_option;
    BitString heading; /* HeadingSlice (16..16) */
    BOOL extent_option;
    Extent extent;
    BOOL position_option;
    FullPositionVector position;
    BOOL furtherInfoID_option;
    OctetString furtherInfoID; /* FurtherInfoID (2..2) */
    BOOL regional_option;
    Reg_RoadSideAlertList regional;
} RoadSideAlert;

#ifdef __cplusplus
}
#endif

#endif /* __J2735_RSA_H__ */
