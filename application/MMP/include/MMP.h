#ifndef MMP_H
#define MMP_H

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "byte_processing.h"
#include "typedefine.h"
#include "config.h"
#include "vms.h"

extern app_obj_t MMP;

int MMP_on_cloud_packet_rx(void *);
int MMP_on_registration(void *);

#endif