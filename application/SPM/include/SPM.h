# ifndef SPM_H
#define SPM_H

#include "typedefine.h"

extern app_obj_t SPM;

int SPM_on_OBU_packet_rx(void *arg);
int SPM_on_registration(void *);

#endif