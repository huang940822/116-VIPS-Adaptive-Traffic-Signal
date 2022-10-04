#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "BSM_codec.h"
#include "ObstacleList.h"
#include "asn1defs_if.h"
#include "j2735_codec.h"
extern pthread_mutex_t lock;
void bsm_print(BasicSafetyMessage *bsm)
{
    int i, fbs;

    printf("Decoded BSM\n");
    printf("  coreData.msgCnt: %u\n", bsm->coreData.msgCnt);
    printf("  coreData.id: ");
    /* the array-like type, such as string or byte array is described by buf and
     * len */
    for (i = 0; i < bsm->coreData.id.len; i++) {
        printf("%hhx", bsm->coreData.id.buf[i]);
    }
    printf("\n");
    printf("  secMark: %d\n", bsm->coreData.secMark);
    printf("  transmission: %d\n", bsm->coreData.transmission);

    /* the data is included only when the optional field is TRUE */
    if (bsm->partII_option) {
        /* the number of items in "sequence of" could be gotten by field,
         * "count"  */
        for (i = 0; i < bsm->partII.count; i++) {
            PartIIcontent *part2 = &(bsm->partII.tab[i]);
            printf("  partII[%d]:\n", i);
            printf("    partII_Id: %d\n", part2->partII_Id);
            switch (part2->partII_Id) {
            case VehicleSafetyExt:
                printf("     case VehicleSafetyExt: ");
                /* by the Find First Set bit function, you can get the flag */
                fbs = asn1_bstr_ffs(&(part2->u.safetyExt->lights));
                switch (fbs) {
                case ExteriorLights_lowBeamHeadlightsOn:
                    printf("ExteriorLights_lowBeamHeadlightsOn\n");
                    break;
                case ExteriorLights_highBeamHeadlightsOn:
                    printf("ExteriorLights_highBeamHeadlightsOn\n");
                    break;
                case ExteriorLights_leftTurnSignalOn:
                    printf("ExteriorLights_leftTurnSignalOn\n");
                    break;
                default:
                    printf("%d\n", fbs);
                }
                break;
            case SpecialVehicleExt:
                break;
            case SupplementalVehicleExt:
                break;
            default:
                break;
            }
        }
    }

    return;
}

int bsm_encode(uint8_t **tx_buf, int *tx_buf_len, Obstacle *obstacle)
{
    MessageFrame msgf;
    BasicSafetyMessage *bsm;
    PartIIcontent *part2_sf_ext;
    PartIIcontent *part2_sp_ext;
    VehicleSafetyExtensions *sf_ext;
    SpecialVehicleExtensions *sp_ext;
    // char id[] = "RSU";
    static int msg_cnt = 0;
    int ret = 1;
    /* all fields which should be allocated before using are allocated
     * recursively */
    bsm = (BasicSafetyMessage *) j2735_msg_prealloc(BasicSafetyMessage_Id);
    if (bsm == NULL) {
        printf("bsm alloc failed!\n");
    }
    bsm->coreData.msgCnt = (msg_cnt++) % 127;
    // printf("message count %d\n", bsm->coreData.msgCnt);

    /* Set fixed id */
    // asn1_ostr_clone_cstr(&(bsm->coreData.id), id, 4);
    char id[5];
    sprintf(id, "%04d", obstacle->ObstacleID);
    asn1_ostr_clone_cstr(&(bsm->coreData.id), id, 4);
    bsm->coreData.secMark = obstacle->second * 1000;
    bsm->coreData.Long = (int) (obstacle->Long);
    bsm->coreData.lat = (int) obstacle->lat;
    bsm->coreData.elev = (int) obstacle->elev;

    asn1_bstr_set_bit(&(bsm->coreData.brakes.wheelBrakes),
                      BrakeAppliedStatus_rightFront);
    bsm->coreData.speed = (int) obstacle->speed;

    bsm->coreData.heading = (int) obstacle->laneID;

    bsm->coreData.size.length = 4;
    bsm->coreData.size.width = (int) obstacle->width;
    /* Set fixed brakes */
    bsm->coreData.brakes.traction = TractionControlStatus_engaged;
    bsm->coreData.brakes.abs = AntiLockBrakeStatus_engaged;
    bsm->coreData.brakes.scs = StabilityControlStatus_engaged;

    /* set the optional field to TRUE to include the data when encoding */
    bsm->partII_option = TRUE;
    /* the items in the list of partII are allocated with max. size */
    /* just set the number of items which are really filled */

    /* Set fixed partII */
    bsm->partII.count = 1;
    /* if the type of the item contains union field, choose the one you want and
     * call prealloc assistant */
    part2_sf_ext = &(bsm->partII.tab[0]);
    part2_sf_ext->partII_Id = VehicleSafetyExt;
    /* all fields which should be allocated before using are allocated
     * recursively */
    j2735_dataframe_prealloc(PartIIcontent_DF, part2_sf_ext);

    /* set union to vehicle safety extensions */
    sf_ext = part2_sf_ext->u.safetyExt;
    /* set the optional field to TRUE to include the data when encoding */
    sf_ext->events_option = TRUE;
    /* only set the bit of bitstring */
    asn1_bstr_set_bit(&(sf_ext->events),
                      VehicleEventFlags_eventStopLineViolation);
    /* set the optional field to TRUE to include the data when encoding */
    sf_ext->pathPrediction_option = TRUE;
    sf_ext->pathPrediction.radiusOfCurve = 1000;
    sf_ext->pathPrediction.confidence = 100;
    /* set the optional field to TRUE to include the data when encoding */
    sf_ext->lights_option = TRUE;
    /* only set the bit of bitstring */
    asn1_bstr_set_bit(&(sf_ext->lights), ExteriorLights_leftTurnSignalOn);

    /* set union to special vehicle extensions */
    // sp_ext = part2_sp_ext->u.specialExt;
    /* set the optional field to TRUE to include the data when encoding */
    // sp_ext->vehicleAlerts_option = TRUE;
    /* set emergency detail content */
    /*sp_ext->vehicleAlerts.sspRights = 0;
    sp_ext->vehicleAlerts.sirenUse = SirenInUse_inUse;
    sp_ext->vehicleAlerts.lightsUse = LightbarInUse_inUse;
    sp_ext->vehicleAlerts.events_option = FALSE;
    sp_ext->vehicleAlerts.responseType_option = TRUE;
    sp_ext->vehicleAlerts.responseType = ResponseType_emergency;*/
    /* set the optional field to FALSE to not include the data when encoding */
    // sp_ext->description_option = FALSE;
    // sp_ext->trailers_option = FALSE;

    /* does the encoding and providing the error msg if there is */
    msgf.messageId = BasicSafetyMessage_Id;
    msgf.u.data = bsm;

    J2735CodecErr *err = (J2735CodecErr *) malloc(sizeof(J2735CodecErr));
    err->msg = (char *) malloc(sizeof(char) * 100);
    err->msg_size = 100;
    *tx_buf_len = j2735_msg_encode(tx_buf, &msgf, err);

    if (*tx_buf_len <= 0) {
        ret = 0;
        printf("failed to encode the msg\n");
        printf("%s\n", err->msg);
        printf("%d\n", err->msg_len);
        printf("%d\n", err->bit_pos);
        printf("%d\n", err->msg_len);
    } else {
        // printf("encode successfully\n");
        // printf("encoded %d byte\n", *tx_buf_len);
        // dump_mem(*tx_buf, *tx_buf_len);
    }
    /* free the memory for encoding */
    for (int i = 0; (i < bsm->partII.count) && (i < PartIIcontentList_MAX_SIZE);
         i++) {
        j2735_dataframe_dealloc(PartIIcontent_DF, &(bsm->partII.tab[i]));
    }
    j2735_msg_dealloc(BasicSafetyMessage_Id, bsm);
    if (err->msg != NULL) {
        free(err->msg);
    }
    if (err != NULL) {
        free(err);
    }
    return ret;
}
int bsm_encode_reg(uint8_t **tx_buf,
                   size_t *tx_buf_len,
                   ObstacleList *obstaclelist)
{
    MessageFrame msgf;
    BasicSafetyMessage *bsm;
    PartIIcontent *part2_sf_ext;
    PartIIcontent *part2_sp_ext;
    VehicleSafetyExtensions *sf_ext;
    SpecialVehicleExtensions *sp_ext;
    static int msg_cnt = 0;
    int ret = 1;

    // memset(&msgf, 0, sizeof(msgf));
    /* all fields which should be allocated before using are allocated
     * recursively */
    bsm = (BasicSafetyMessage *) j2735_msg_prealloc(BasicSafetyMessage_Id);
    if (bsm == NULL) {
        printf("bsm alloc failed!\n");
    }
    bsm->coreData.msgCnt = (msg_cnt++) % 127;

    bsm->coreData.secMark = 0;
    bsm->coreData.Long = 0;
    bsm->coreData.lat = 0;
    bsm->coreData.heading = obstaclelist->dirct;
    asn1_bstr_set_bit(&(bsm->coreData.brakes.wheelBrakes),
                      BrakeAppliedStatus_rightFront);
    bsm->coreData.speed = 0;
    /* Set fixed brakes */
    bsm->coreData.brakes.traction = TractionControlStatus_engaged;
    bsm->coreData.brakes.abs = AntiLockBrakeStatus_engaged;
    bsm->coreData.brakes.scs = StabilityControlStatus_engaged;

    bsm->partII_option = FALSE;

    /*regional data*/
    Reg_BasicSafetyMessage *reg_bsm =
        (Reg_BasicSafetyMessage *) malloc(sizeof(Reg_BasicSafetyMessage));

    bsm->regional_option = TRUE;
    bsm->regional.count = 1;
    bsm->regional.tab = reg_bsm;

    bsm->regional.tab[0].regionId = 255;  // NoRegion;
    bsm->regional.tab[0].u.unknown.buf = (uint8_t *) obstaclelist->tab;
    bsm->regional.tab[0].u.unknown.len = obstaclelist->count * sizeof(Obstacle);

    /* does the encoding and providing the error msg if there is */
    msgf.messageId = BasicSafetyMessage_Id;
    msgf.u.data = bsm;

    J2735CodecErr *err = (J2735CodecErr *) malloc(sizeof(J2735CodecErr));
    err->msg = (char *) malloc(sizeof(char) * 100);
    err->msg_size = 100;

    *tx_buf_len = j2735_msg_encode(tx_buf, &msgf, err);

    if (*tx_buf_len <= 0) {
        ret = 0;
        printf("failed to encode the msg\n");
        printf("%s\n", err->msg);
        printf("%d\n", err->msg_len);
        printf("%d\n", err->bit_pos);
        printf("%d\n", err->msg_len);
    } else {
        // printf("encode successfully\n");
        // printf("encoded %d byte\n", *tx_buf_len);
        // dump_mem(*tx_buf, *tx_buf_len);
    }
    /* free the memory for encoding */
    for (int i = 0; (i < bsm->partII.count) && (i < PartIIcontentList_MAX_SIZE);
         i++) {
        j2735_dataframe_dealloc(PartIIcontent_DF, &(bsm->partII.tab[i]));
    }
    j2735_msg_dealloc(BasicSafetyMessage_Id, bsm);

    free(err->msg);
    free(err);
    return ret;
}
void bsm_decode(uint8_t *rx_buf, int rx_buf_len)
{
    int ret;
    /* a pointer to containing decoded msg */
    MessageFrame *p_msgf;

    printf("BSM decoding data:\n");
    J2735CodecErr *err = (J2735CodecErr *) malloc(sizeof(J2735CodecErr));
    err->msg = (char *) malloc(sizeof(char) * 20);
    ret = j2735_msg_decode(&p_msgf, rx_buf, rx_buf_len, err);
    if (ret < 0) {
        /* handling the decoding error */
        printf("decode msg error\n");
        printf("%s\n", err->msg);
    } else if ((ret > 0) && (p_msgf->messageId == BasicSafetyMessage_Id)) {
        // bsm_print((BasicSafetyMessage *)(p_msgf->u.data));
        J2735_FREE_MSG_FRAME(p_msgf);
    }

    return;
}
