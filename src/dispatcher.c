#include <stdio.h>

#include "log.h"
#include "server.h"
#include "msg_queue.h"
#include "dispatcher.h"
#include "com_packet_processing.h"

uint8_t cloud_com_id;
uint8_t OBU_com_id;
uint8_t Heartbeat_com_id;
uint8_t AVI_com_id;


// which will continuously dequeue message objects form the message queue
void* dispatcher_handler()
{
	msg_queue_init();

	int ret = 0;
	char log_content[LOG_CONTENT_LEN + 1];

	struct msg_obj* msg;
	for (;;) {
		memset(log_content, 0, sizeof(log_content));
		msg = msg_queue_dequeue();
		snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "dispatcher: MSG(%d)", msg->device_id);
		log_file_write(log_content);
		if (msg->device_id == FROM_CLOUD) {
			// printf("cloud_rx_event\n");
			
			cloud_com_id = msg->handle_id;
			memset(log_content, 0, sizeof(log_content));
			snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "cloud_com_id in dispatcher is %d", cloud_com_id);
			log_file_write(log_content);
			ret = cloud_packet_rx_event_handler(msg);
			if (ret < 0) {
				log_file_write_fatal_error("invalid packet from cloud: %d\n", ret);
			}
		}
		if (msg->device_id == FROM_DSRC) {
			// printf("OBU_rx_event\n");
			if (Is_Heartbeat(msg) == 1){
				Heartbeat_com_id = msg->handle_id;
			}
			else{
				OBU_com_id = msg->handle_id;
				pthread_t APP_thread;
				//int ret = pthread_create(&APP_thread, NULL, OBU_packet_rx_event_handler, "Child");
				ret = OBU_packet_rx_event_handler(msg);
				if (ret < 0) {
					log_file_write_fatal_error("invalid packet from OBU: %d\n", ret);
				}
			}
		}
		if (msg->device_id == FROM_SMART_AVI) {
 			AVI_com_id = msg->handle_id;
 			Smart_AVI_packet_rx_event_handler(msg);		
 		}
		free(msg);
	}
	log_file_write_fatal_error("dispatcher thread exit");
}
