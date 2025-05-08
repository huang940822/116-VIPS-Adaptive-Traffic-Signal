#ifndef PEDESTRIAN_H
#define PEDESTRIAN_H


//#include <libwebsockets.h>
#include "typedefine.h"


extern app_obj_t PEDESTRIAN;
extern struct lws_context* context;

int PEDESTRIAN_on_pedestrian_packet_rx(void *);
int PEDESTRIAN_on_registration(void *);

/*�t��kfunction�ŧi*/
void init_variable(void);
void calculateMaxPed(void);
void calculatePassPed(void);
void alg1(void);
void alg2(void);
void alg3(void);
int runPedestrianSignalControl(void);

#endif