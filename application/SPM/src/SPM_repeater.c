#include "SPM_repeater.h"
#include "OBU_record_processing.h"
#include "SPM.h"
#include "SPM_OBU_list.h"
#include "SPM_config.h"
#include "com_packet_processing.h"
#include "config.h"
#include "j2735_codec.h"
#include "log.h"
#include "vector.h"

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
int SPM_repeater_fd = 0;

uint32_t send_packet_num = 0;
void sigintHandlerSPM(int sig_num)
{
    printf("send_packet_num %d\n", send_packet_num);
    exit(0);
}

void SPM_repeater_start(bool send_flag)
{
    pthread_mutex_lock(&SPM_repeater_run_mutex);
    if (SPM_repeater_thread == 0) {
        int ret = pthread_create(&SPM_repeater_thread, NULL, SPM_repeater, NULL);
        if (ret != 0) {
            log_file_write_fatal_error("error creating SPM_repeater_thread: %d", ret);
            perror("main: pthread_create");
            exit(errno);
        }
    } else if (send_flag) {
        struct itimerspec timerValue;
        memset(&timerValue, 0, sizeof(struct itimerspec));

        timerValue.it_value.tv_sec = 0;
        timerValue.it_value.tv_nsec = 1;
        timerValue.it_interval.tv_sec = 1 / SPM_config.SPM_packet_transfer_speed;
        timerValue.it_interval.tv_nsec = (int) (1000000000 / SPM_config.SPM_packet_transfer_speed) % 1000000000;

        if (timerfd_settime(SPM_repeater_fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
            log_file_write_fatal_error("SPM_repeater timerfd_settime");
        }
    }
    pthread_mutex_unlock(&SPM_repeater_run_mutex);
}

void *SPM_repeater()
{
    SPM_repeater_fd = timerfd_create(CLOCK_REALTIME, 0);
    int s;
    uint64_t exp;

    struct itimerspec timerValue;
    memset(&timerValue, 0, sizeof(struct itimerspec));

    timerValue.it_value.tv_sec = 1 / SPM_config.SPM_packet_transfer_speed;
    timerValue.it_value.tv_nsec = (int) (1000000000 / SPM_config.SPM_packet_transfer_speed) % 1000000000;
    timerValue.it_interval.tv_sec = 1 / SPM_config.SPM_packet_transfer_speed;
    timerValue.it_interval.tv_nsec = (int) (1000000000 / SPM_config.SPM_packet_transfer_speed) % 1000000000;

    printf("SPM_repeater timerfd_settime %ld %ld\n", timerValue.it_interval.tv_sec, timerValue.it_interval.tv_nsec);
    if (timerfd_settime(SPM_repeater_fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
        log_file_write_fatal_error("SPM_repeater timerfd_settime");
        goto SPM_repeater_end;
    }

    vector_t(SignalStatusMessage *) ssm_ptrv;
    vector_init(ssm_ptrv);

    int32_t sequenceNumber = 0;

    while (!SPM.dontSend2TC) {
        s = read(SPM_repeater_fd, &exp, sizeof(uint64_t));

        pthread_mutex_lock(&SPM_OBU_obj_mutex);
        SPM_OBU_obj_t *current = list_entry(SPM_OBU_list_head.next, SPM_OBU_obj_t, node);
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = (time_t) tv.tv_sec;

        struct tm *timeinfo = localtime(&tv.tv_sec);
        int ssm_ptrv_index = 0;

        while (&current->node != &SPM_OBU_list_head) {
            SignalStatusMessage *ssm;
            while (ssm_ptrv_index >= vector_size(ssm_ptrv)) {
                ssm = (SignalStatusMessage *) j2735_msg_prealloc(SignalStatusMessage_Id);
                ssm->timeStamp_option = TRUE;
                /* 先填入RSU id */
                for (int i = 0; i < SignalStatusList_MAX_SIZE; i++) {
                    ssm->status.tab[i].id.id = config.RSU_id;
                    ssm->status.tab[i].id.region_option = TRUE;
                    ssm->status.tab[i].id.region = config.RSU_region;
                }
                vector_push_back(ssm_ptrv, ssm);
            }
            ssm = vector_at(ssm_ptrv, ssm_ptrv_index++);

            ssm->timeStamp = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;
            ssm->second = (timeinfo->tm_sec * 1000) + (tv.tv_usec / 1000);

            ssm->status.count = 0;
            // 給測試用的
            ssm->regional_option = true;
            ssm->regional.count = 1;
            ssm->regional.tab->u.unknown.buf = (uint8_t *) &tv;
            ssm->regional.tab->u.unknown.len = sizeof(struct timeval);

            while (&current->node != &SPM_OBU_list_head && ssm->status.count < SignalStatusList_MAX_SIZE) {
                SignalStatus *status = &ssm->status.tab[ssm->status.count++];
                status->sigStatus.count = 0;

                status->sequenceNumber = sequenceNumber++;
                sequenceNumber &= 0b1111111;  // mod

                while (&current->node != &SPM_OBU_list_head && status->sigStatus.count < SignalStatusList_MAX_SIZE) {
                    if (now - current->time_second > 1) {
                        current = SPM_OBU_obj_delete(current);
                        continue;
                    }

                    for (int j = 0; j <= current->sigRequest_count && status->sigStatus.count < SignalStatusList_MAX_SIZE; j++) {
                        SignalStatusPackage *ssp = &status->sigStatus.tab[status->sigStatus.count++];
                        memset(ssp, 0, sizeof(SignalStatusPackage));

                        switch (current->sigRequestList[j].request.requestType) {
                        case PriorityRequestType_priorityRequest:
                            ssp->status = PrioritizationResponseStatus_requested;
                            break;
                        case PriorityRequestType_priorityRequestUpdate: {
                            switch (special_OBU_list_search_status(current->vehicle_type, current->OBU_name)) {
                            case OBU_object_unknown:
                                status->sigStatus.count--;
                                continue;
                                break;
                            case OBU_object_processing:
                                ssp->status = PrioritizationResponseStatus_processing;
                                break;
                            case OBU_object_granted: {
                                ssp->status = PrioritizationResponseStatus_granted;
                                // 如果同方向都是 granted 第二個會是 reserviceLocked
                                for (int k = 0; k < status->sigStatus.count - 1; k++) {
                                    if (status->sigStatus.tab[k].status == PrioritizationResponseStatus_granted) {
                                        ssp->status = PrioritizationResponseStatus_reserviceLocked;
                                        break;
                                    }
                                }
                            } break;
                            case OBU_object_rejected:
                                ssp->status = PrioritizationResponseStatus_rejected;
                                break;
                            default:
                                break;
                            }
                        } break;
                        case PriorityRequestType_priorityRequestTypeReserved:
                        case PriorityRequestType_priorityCancellation:
                            status->sigStatus.count--;
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
                    current = list_entry(current->node.next, SPM_OBU_obj_t, node);
                    // current = SPM_OBU_obj_delete(current);
                }
                if (status->sigStatus.count == 0)
                    ssm->status.count--;
            }
        }


        pthread_mutex_unlock(&SPM_OBU_obj_mutex);

        for (int i = 0; i < ssm_ptrv_index; i++) {
            SignalStatusMessage *ssm = vector_at(ssm_ptrv, i);
            if (ssm->status.count == 0)
                break;
            OBU_j2735_tx(SignalStatusMessage_Id, ssm);
            send_packet_num++;
        }
    }
SPM_repeater_end:
    pthread_mutex_lock(&SPM_repeater_run_mutex);
    SPM_repeater_thread = 0;
    close(SPM_repeater_fd);
    SPM_repeater_fd = 0;
    pthread_mutex_unlock(&SPM_repeater_run_mutex);
    for (int i = 0; i < vector_size(ssm_ptrv); i++) {
        j2735_msg_dealloc(SignalStatusMessage_Id, vector_at(ssm_ptrv, i));
    }
    vector_free(ssm_ptrv);
    printf("SPM_repeater_thread end\n");
    pthread_detach(pthread_self());
}
