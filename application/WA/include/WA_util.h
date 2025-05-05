#ifndef WA_UTIL_H
#define WA_UTIL_H

#include "WA.h"

static inline int max_int(int a, int b){
    return (a>b) ? a : b;
}
void WA_get_leading_vehicles(CCI *CCI_input);
void WA_clear_leading_vehicles();
bool is_valid_gps(double lat, double lon);
bool is_valid_speed(double speed);


#endif