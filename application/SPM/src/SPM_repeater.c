#include "SPM_repeater.h"
#include "SPM.h"
#include "SPM_OBU_list.h"
#include "SPM_config.h"
#include "com_packet_processing.h"
#include "config.h"
#include "j2735_codec.h"
#include "log.h"
#include "OBU_record_processing.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include "sys/time.h"

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

    timerValue.it_value.tv_sec = 1 / SPM_config.SPM_packet_transfer_speed;
    timerValue.it_value.tv_nsec = 1000000000 / SPM_config.SPM_packet_transfer_speed;
    timerValue.it_interval.tv_sec = 1 / SPM_config.SPM_packet_transfer_speed;
    timerValue.it_interval.tv_nsec = 1000000000 / SPM_config.SPM_packet_transfer_speed;

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
        log_file_write_fatal_error("SPM_repeater timerfd_settime");
        goto SPM_repeater_end;
    }

    SignalStatusMessage *ssm;
    J2735CodecErr j2735_err;
    int32_t sequenceNumber = 0;

    ssm = (SignalStatusMessage *) j2735_msg_prealloc(SignalStatusMessage_Id);

    ssm->timeStamp_option = TRUE;
    for (int i = 0; i < SignalStatusList_MAX_SIZE; i++) {
        ssm->status.tab[i].id.id = config.RSU_id;
        ssm->status.tab[i].id.region_option = TRUE;
        ssm->status.tab[i].id.region = config.RSU_region;
    }
    int delete_OBU_num = 0, i = 1;
    char delete_OBU_names[SignalStatusList_MAX_SIZE][OBU_NAME_MAX_LEN + 1] = {0};

    ssm->status.count = 1;

    while (!SPM.dontSend2TC && i != 0) {
        s = read(fd, &exp, sizeof(uint64_t));

        i = 0;
        delete_OBU_num = 0;
        pthread_mutex_lock(&SPM_OBU_obj_mutex);
        SPM_OBU_obj_t *current = SPM_OBU_obj_head;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = (time_t) tv.tv_sec;

        struct tm *timeinfo = localtime(&tv.tv_sec);
        ssm->timeStamp = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;
        ssm->second = (timeinfo->tm_sec * 1000) + (tv.tv_usec / 1000);

        ssm->status.tab[0].sequenceNumber = sequenceNumber++;
        sequenceNumber &= 0b1111111;

        while (current != NULL && i <= SignalStatusList_MAX_SIZE) {
            if (now - current->time_second > SPM_config.spm_host_obu_packet_timeout) {
                memcpy(delete_OBU_names[delete_OBU_num], current->OBU_name, OBU_NAME_MAX_LEN + 1);
                delete_OBU_num++;
                current = current->next;
                continue;
            }

            for (int j = 0; j <= current->sigRequest_count && i <= SignalStatusList_MAX_SIZE ; j++) {
                SignalStatusPackage *ssp = &ssm->status.tab[0].sigStatus.tab[i++];
                memset(ssp, 0, sizeof(SignalStatusPackage));

                switch (current->sigRequestList[j].request.requestType) {
                case PriorityRequestType_priorityRequest: 
                    ssp->status = PrioritizationResponseStatus_requested;
                    break;
                case PriorityRequestType_priorityRequestUpdate: {
                    static int OBU_object_status[] = {PrioritizationResponseStatus_processing, PrioritizationResponseStatus_granted, 
                        PrioritizationResponseStatus_rejected};
                    int status = special_OBU_list_search_status(current->vehicle_type, current->OBU_name);
                    if (status == OBU_object_unknown) {
                        i--;
                        continue;
                    }
                    ssp->status = OBU_object_status[status];
                } break;
                case PriorityRequestType_priorityRequestTypeReserved:
                case PriorityRequestType_priorityCancellation:
                    i--;
                    continue;
                    break;
                }

                ssp->requester_option = TRUE;
                ssp->requester.id.choice = current->id.choice;
                if (current->id.choice == VehicleID_entityID)
                    asn1_ostr_clone_cstr(&ssp->requester.id.u.entityID, current->id.u.buf, 4);
                else
                    ssp->requester.id.u.stationID = current->id.u.stationID;

                ssp->requester.request = current->sigRequestList[j].request.requestID;
                ssp->requester.role_option = TRUE;
                ssp->requester.role = current->role;

                memcpy(&ssp->inboundOn, &current->sigRequestList[j].request.inBoundLane, sizeof(IntersectionAccessPoint));
                if (current->sigRequestList[j].request.outBoundLane_option) {
                    ssp->outboundOn_option = TRUE;
                    memcpy(&ssp->outboundOn, &current->sigRequestList[j].request.outBoundLane, sizeof(IntersectionAccessPoint));
                }

                if (current->sigRequestList[j].minute_option) {
                    ssp->minute_option = TRUE;
                    ssp->minute = current->sigRequestList[j].minute;
                }

                if (current->sigRequestList[j].second_option) {
                    ssp->second_option = TRUE;
                    ssp->second = current->sigRequestList[j].second;
                }

                if (current->sigRequestList[j].duration_option) {
                    ssp->duration_option = TRUE;
                    ssp->duration = current->sigRequestList[j].duration;
                }
            }
            current = current->next;
        }
        pthread_mutex_unlock(&SPM_OBU_obj_mutex);
        ssm->status.tab[0].sigStatus.count = i;
        if (ssm->status.tab[0].sigStatus.count > 0)
            OBU_j2735_tx(SignalStatusMessage_Id, ssm);

        for (int j = 0; j < delete_OBU_num; j++) {
            SPM_OBU_obj_delete(delete_OBU_names[j]);
        }
    }
SPM_repeater_end:
    SPM_repeater_thread = 0;
    close(fd);
    pthread_detach(pthread_self());
}
