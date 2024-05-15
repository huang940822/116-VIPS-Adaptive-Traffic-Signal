#ifndef CMS_H
#define CMS_H

#include "vms.h"

#define CMS_PORT 20204
#define CMS_INTERFACE_NAME "enp2s0"
#define CMS_pic_path VMS_pic_path
#define CMS_pic_database_path VMS_pic_database_path
#define CMS_encrypt_img "tmp_encrypt_img"
#define CMS_scp_path "asrlab@"
#define CMS_update_fail_time 10
#define CMS_request_timeout 60
#define CMS_NUM_MAX 16

void CMS_handler_init();
int CMS_request_start(int appID, uint8_t priority, uint8_t display_buffer[CMS_NUM_MAX]);
int CMS_request_end(int appID);
int CMS_update_activate(int Program_ID, char Program_Name[100]);
int CMS_check_img(char imgName[100]);
#endif