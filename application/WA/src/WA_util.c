#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WA_config.h"
#include "WA_util.h"
#include "util.h"

LOG_USE_MODULE(WA);

extern CCI Leading_Vehicles[4];
extern pthread_mutex_t mutex_LV;

void WA_get_leading_vehicles(CCI *CCI_INPUT){
    pthread_mutex_lock(&mutex_LV);
    for(int i = 0; i < 4; i++){
        CCI_INPUT[i] = Leading_Vehicles[i];
    }
    pthread_mutex_unlock(&mutex_LV);
}
void WA_clear_leading_vehicles(){
    pthread_mutex_lock(&mutex_LV);
    for(int i = 0; i < 4; i++){
        Leading_Vehicles[i].speed = -1.0;
        Leading_Vehicles[i].distance = -1.0;
    }
    pthread_mutex_unlock(&mutex_LV);
}
bool is_valid_gps(double lat, double lon) {
    return (lat >= -90.0 && lat <= 90.0 &&
            lon >= -180.0 && lon <= 180.0 &&
            !(lat == -1.0 && lon == -1.0));
}
bool is_valid_speed(double speed) {
    return speed >= 0.0;
}