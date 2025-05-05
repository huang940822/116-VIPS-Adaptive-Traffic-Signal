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

LOG_USE_MODULE(SPM);

pthread_t SPM_repeater_thread = 0;
pthread_mutex_t SPM_repeater_run_mutex = PTHREAD_MUTEX_INITIALIZER;
int SPM_repeater_fd = 0;

void SPM_repeater_start(bool send_flag)
{
    pthread_mutex_lock(&SPM_repeater_run_mutex);
    if (SPM_repeater_thread == 0) {
        int ret = pthread_create(&SPM_repeater_thread, NULL, SPM_repeater, NULL);
        if (ret != 0) {
            LOG_MSG_FATAL("error creating SPM_repeater_thread: %d", ret);
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
            LOG_MSG_FATAL("SPM_repeater timerfd_settime");
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

    LOG_MSG_TRACE("SPM_repeater timerfd_settime %ld %ld", timerValue.it_interval.tv_sec, timerValue.it_interval.tv_nsec);
    if (timerfd_settime(SPM_repeater_fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
        LOG_MSG_FATAL("SPM_repeater timerfd_settime");
        goto SPM_repeater_end;
    }

    vector_t(SignalStatusMessage *) ssm_ptrv;
    vector_init(ssm_ptrv);

    int32_t sequenceNumber = 0;

    while (1) {
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
            while (&current->node != &SPM_OBU_list_head && ssm->status.count < SignalStatusList_MAX_SIZE) {
                SignalStatus *status = &ssm->status.tab[ssm->status.count++];
                status->sigStatus.count = 0;

                status->sequenceNumber = sequenceNumber++;
                sequenceNumber &= 0b1111111;  // mod

                while (&current->node != &SPM_OBU_list_head && status->sigStatus.count < SignalStatusList_MAX_SIZE) {
                    if (now - current->time_second > 20) {
                        current = SPM_OBU_obj_delete(current);
                        continue;
                    }

                    for (int j = 0; j <= current->sigRequest_count && status->sigStatus.count < SignalStatusList_MAX_SIZE; j++) {
                        SignalStatusPackage *ssp = &status->sigStatus.tab[status->sigStatus.count++];
                        /*
                        每一個 SignalStatusPackage 都是算一次請求所以在一個 SSM 裡面可以有很多個請求
                        會有三個階段 HandShake, Pooling, Close
                        */
                        switch (current->sigRequestList[j].request.requestType) {
                        case PriorityRequestType_priorityRequest: // HandShake
                            ssp->status = PrioritizationResponseStatus_requested;
                            break;
                        case PriorityRequestType_priorityRequestUpdate: { // Pooling
                            switch (special_OBU_list_search_status(current->vehicle_type, current->OBU_name)) {
                            case OBU_object_unknown:
                                status->sigStatus.count--;
                                continue;
                                break;
                            case OBU_object_processing:
                                ssp->status = PrioritizationResponseStatus_processing;
                                break;
                            case OBU_object_granted: {
                                // 會在成功更新 command buf 後 granted
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
                        case PriorityRequestType_priorityCancellation: // Close
                            status->sigStatus.count--;
                            continue;
                            break;
                        }

                        ssp->requester_option = TRUE;
                        ssp->requester.id.choice = VehicleID_stationID;
                        if (current->id.choice == current->id.choice)
                            ssp->requester.id.u.stationID = *(uint32_t *) current->id.u.buf;
                        else
                            ssp->requester.id.u.stationID = current->id.u.stationID;

                        ssp->requester.request = current->sigRequestList[j].request.requestID;
                        ssp->requester.role_option = TRUE;
                        ssp->requester.role = current->role;

                        memcpy(&ssp->inboundOn, &current->sigRequestList[j].request.inBoundLane, sizeof(IntersectionAccessPoint));
                        if (current->sigRequestList[j].request.outBoundLane_option) {
                            ssp->outboundOn_option = TRUE;
                            memcpy(&ssp->outboundOn, &current->sigRequestList[j].request.outBoundLane, sizeof(IntersectionAccessPoint));
                        } else {
                            ssp->outboundOn_option = FALSE;
                        }

                        if (current->sigRequestList[j].minute_option) {
                            ssp->minute_option = TRUE;
                            ssp->minute = current->sigRequestList[j].minute;
                        } else {
                            ssp->minute_option = FALSE;
                        }

                        if (current->sigRequestList[j].second_option) {
                            ssp->second_option = TRUE;
                            ssp->second = current->sigRequestList[j].second;
                        } else {
                            ssp->second_option = FALSE;
                        }

                        if (current->sigRequestList[j].duration_option) {
                            ssp->duration_option = TRUE;
                            ssp->duration = current->sigRequestList[j].duration;
                        } else {
                            ssp->duration_option = FALSE;
                        }
                    }
                    current = list_entry(current->node.next, SPM_OBU_obj_t, node);
                    // current = SPM_OBU_obj_delete(current);
                }
                if (status->sigStatus.count == 0)
                    ssm->status.count--;
            }
            if (ssm->status.count == 0)
                ssm_ptrv_index--;
        }
        pthread_mutex_unlock(&SPM_OBU_obj_mutex);
        if (ssm_ptrv_index == 0)
            goto SPM_repeater_end;
        for (int i = 0; i < ssm_ptrv_index; i++) {
            SignalStatusMessage *ssm = vector_at(ssm_ptrv, i);
            if (ssm->status.count == 0)
                break;
            OBU_j2735_tx(SignalStatusMessage_Id, ssm);
        }
    }
SPM_repeater_end:
    pthread_mutex_lock(&SPM_repeater_run_mutex);
    SPM_repeater_thread = 0;
    close(SPM_repeater_fd);
    SPM_repeater_fd = 0;
    pthread_mutex_unlock(&SPM_repeater_run_mutex);
    for (int i = 0; i < vector_size(ssm_ptrv); i++) {
        SignalStatusMessage *ssm = vector_at(ssm_ptrv, i);
        j2735_msg_dealloc(SignalStatusMessage_Id, ssm);
    }
    vector_free(ssm_ptrv);
    LOG_MSG_INFO("SPM_repeater_thread end");
    pthread_detach(pthread_self());
}
