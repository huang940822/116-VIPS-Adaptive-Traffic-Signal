#include "SPM.h"
#include "SPM_OBU_list.h"
#include "SPM_config.h"
#include "SPM_repeater.h"
#include "application_registration.h"
#include "config.h"
#include "j2735_msg.h"
#include "j2735_srm.h"
#include "log.h"

#include <stdio.h>

app_obj_t SPM = {
    .name = "SPM",
    .id = 7,
    .priority = 3,
    .on_registration = &SPM_on_registration,
    .dontSend2TC = 1,
};

int SPM_on_OBU_packet_rx(void *arg)
{
    V2R_app_section_t *app_section = (V2R_app_section_t *) arg;
    if (app_section->msgID != SignalRequestMessage_Id)
        return -1;
    SignalRequestMessage *srm = (SignalRequestMessage *) app_section->data;

    if (srm->requests_option) {
        int i = 0;
        for (i; i < srm->requests.count; i++) {
            if (srm->requests.tab[i].request.id.id == config.RSU_id) {
                if (srm->requests.tab[i].request.id.region_option) {
                    if (srm->requests.tab[i].request.id.region == config.RSU_region)
                        break;
                } else
                    break;
            }
        }
        if (i == srm->requests.count)
            return -1;
    } else {
        return -1;
    }
    if (SPM.dontSend2TC)
        return 1;
    printf("ededede-----\n");
    SPM_OBU_obj_insert(app_section->OBU_object, srm);
    SPM_repeater_start();
}

int SPM_on_registration(void *arg)
{
    int ret = SPM_config_init();
    if (ret != 0) {
        log_file_write_fatal_error("error spm reading config file: %d", ret);
    }
    SPM.dontSend2TC = SPM_config.SPM_dontSend2TC;
    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, SPM.name, SPM.priority, SignalRequestMessage_Id, &SPM_on_OBU_packet_rx);
    return 1;
}