#include "SPM_repeater.h"
#include "SPM.h"
#include "log.h"
#include "SPM_OBU_list.h"
#include "config.h"
#include "j2735_codec.h"
#include "com_packet_processing.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/timerfd.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

pthread_t SPM_repeater_thread = 0;
pthread_mutex_t SPM_repeater_run_mutex = PTHREAD_MUTEX_INITIALIZER;

void SPM_repeater_start()
{
    pthread_mutex_lock(&SPM_repeater_run_mutex);
    if (SPM_repeater_thread == 0) {
        int ret = pthread_create(&SPM_repeater_thread, NULL, SPM_repeater, NULL);
        if (ret != 0) {
            log_file_write_fatal_error("error creating SPM_repeater_thread: %d", ret);
            perror("main: pthread_create");
            exit(errno);
        }
    }
    pthread_mutex_unlock(&SPM_repeater_run_mutex);
}

void *SPM_repeater()
{
    int fd = timerfd_create(CLOCK_REALTIME, 0), s;
    uint64_t exp;

    struct itimerspec timerValue;
    memset(&timerValue, 0, sizeof(struct itimerspec));

    timerValue.it_value.tv_sec = 0;
    timerValue.it_value.tv_nsec = 100000000;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = 100000000;

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
        log_file_write_fatal_error("SPM_repeater timerfd_settime");
        SPM_repeater_thread = 0;
        return NULL;
    }

    SignalStatusMessage *ssm;
    J2735CodecErr j2735_err;
    int32_t sequenceNumber = 0;

    ssm = (SignalStatusMessage *)j2735_msg_prealloc(SignalStatusMessage_Id);
    
    ssm->timeStamp_option = TRUE;
    for (int i = 0 ;i < SignalStatusList_MAX_SIZE; i++) {
        ssm->status.tab[i].id.id = config.RSU_id;
        ssm->status.tab[i].id.region_option = TRUE;
        ssm->status.tab[i].id.region = config.RSU_region;
    }


    while (SPM.dontSend2TC) {
        s = read(fd, &exp, sizeof(uint64_t));

        int i = 0;
        pthread_mutex_lock(&SPM_OBU_obj_mutex);
        SPM_OBU_obj_t *current = SPM_OBU_obj_head;
        while (current) {
            ssm->status.tab[i].sequenceNumber = current->sequenceNumber;
            ssm->status.tab[i].sigStatus.count = current->sigRequest_count;
            for (int j = 0; j < current->sigRequest_count; j++) {
                SignalStatusPackage *ssp = &ssm->status.tab[i].sigStatus.tab[j];
                memset(ssp, 0, sizeof(SignalStatusPackage));

                ssp->requester_option = TRUE;
                ssp->requester.id.choice = current->id.choice;
                if (current->id.choice == VehicleID_entityID)
                    asn1_ostr_clone_cstr(&ssp->requester.id.u.entityID, current->id.u.buf, 4);
                else
                    ssp->requester.id.u.stationID = current->id.u.stationID;
                ssp->requester.role_option = TRUE;
                ssp->requester.role = current->role;

                memcpy(&ssp->inboundOn, &current->sigRequestList[i].request.inBoundLane, sizeof(IntersectionAccessPoint));
                if (current->sigRequestList[i].request.outBoundLane_option) {
                    ssp->outboundOn_option = TRUE;
                    memcpy(&ssp->outboundOn, &current->sigRequestList[i].request.outBoundLane, sizeof(IntersectionAccessPoint));
                }
                
                if (current->sigRequestList[i].minute_option) {
                    ssp->minute_option = TRUE;
                    ssp->minute = current->sigRequestList[i].minute;
                }

                if (current->sigRequestList[i].second_option) {
                    ssp->second_option = TRUE;
                    ssp->second = current->sigRequestList[i].second;
                }

                if (current->sigRequestList[i].duration_option) {
                    ssp->duration_option = TRUE;
                    ssp->duration = current->sigRequestList[i].duration;
                }
            }

            i++;
            current = current->next;
        }
        pthread_mutex_unlock(&SPM_OBU_obj_mutex);
        ssm->status.count = i;

        OBU_j2735_tx(SignalStatusMessage_Id, ssm);
    }
    SPM_repeater_thread = 0;
}
