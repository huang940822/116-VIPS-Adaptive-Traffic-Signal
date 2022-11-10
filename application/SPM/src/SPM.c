#include "SPM.h"
#include "application_registration.h"
#include "j2735_msg.h"
#include "j2735_srm.h"
#include "config.h"

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
    if(app_section->msgID != SignalRequestMessage_Id)
        return -1;
    SignalRequestMessage *srm = (SignalRequestMessage *)app_section->data;

    /* If SRM checks whether this message is for self. */
    if (app_section->msgID == SignalRequestMessage_Id) {
        SignalRequestMessage *srm = app_section->data;
        if (srm->requests_option) {
            int i = 0;
            for (i; i < srm->requests.count; i++) {
                if (srm->requests.tab[i].request.id.id == config.RSU_id)
                    break;
            }
            if (i == srm->requests.count)
                return -1;
        }
        else {
            return -1;
        }
    }

    
}

int SPM_on_registration(void *arg)
{
    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, SPM.name, SPM.priority, SignalRequestMessage_Id, &SPM_on_OBU_packet_rx);
    return 1;
}