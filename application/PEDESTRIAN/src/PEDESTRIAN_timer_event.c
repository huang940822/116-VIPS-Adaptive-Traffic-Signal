#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include <libwebsockets.h>         
#include <signal.h>                
#include "PEDESTRIAN.h"
#include "PEDESTRIAN_timer_event.h"
#include "log.h"

void PEDESTRIAN_Agent_timer_handler(__sigval_t value) {
    if(context) {
        lws_service(context, 0);
    }
    // log_file_write("[PEDESTRIAN] LWS SERVICE triggered");
    
}