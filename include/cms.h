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

void CMS_handler_init();

#endif