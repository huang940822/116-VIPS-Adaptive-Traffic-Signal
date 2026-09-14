#ifndef PEDESTRIAN_PLUS_H  
#define PEDESTRIAN_PLUS_H

#include <libwebsockets.h>
#include "typedefine.h"

extern app_obj_t PEDESTRIAN_PLUS;
extern struct lws_context* context_plus; 

int PEDESTRIAN_plus_on_pedestrian_packet_rx(void*);
int PEDESTRIAN_plus_on_registration(void*);
void pedestrian_plus_timer_handler(union sigval sv);
int create_pedestrian_plus_timer(void);
int start_pedestrian_plus_timer(void);
void init_variable_plus(void);
void calculateMaxPed_plus(void);
void calculatePassPed_plus(void);
void alg1_plus(void);
void alg2_plus(void);
void alg3_plus(void);
int runPedestrianSignalControl_plus(void);

#endif