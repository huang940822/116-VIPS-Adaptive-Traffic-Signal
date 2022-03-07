#include "ae_timer_event.h"


/*This is just a test function that display current time*/
int time_print_cur_time(ae_event_loop *event_loop,
                        long long id,
                        void *clientData)
{
    if (clientData != NULL) {
        int *clock_timer_id = (int *) clientData;
        *clock_timer_id = id;
    }
    time_t mytime = time(NULL);
    char *time_str = ctime(&mytime);
    time_str[strlen(time_str) - 1] = '\0';
    // printf("Current Time : %s\n", time_str);
    /*The four functions asctime(), ctime(), gmtime() and localtime() return a
     * pointer to static data and hence are not thread-safe.*/
    /*ctime() returns a pointer to a static buffer, and must not be free()*/
    return SEC_TO_MSEC(5);
}


int on_cloud_disconnected(ae_event_loop *event_loop,
                          long long id,
                          void *clientData)
{
    printf("on_cloud_disconnected\n");
    if (clientData != NULL) {
        int *cloud_expired_id = (int *) clientData;
        *cloud_expired_id = id;
    }
    err_msg = malloc(sizeof(struct msg_obj));

    SET_DEVICE_ID(err_msg->device_id, CLOUD_BIT_POS);
    SET_DEVICE_DISCONNECTED(err_msg->device_id);

    msg_queue_enqueue(err_msg);
    return AE_ONESHOT;
}