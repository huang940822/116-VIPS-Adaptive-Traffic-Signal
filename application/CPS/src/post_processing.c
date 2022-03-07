#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "CPS.h"
#include "ObstacleList.h"
#include "post_processing.h"

void transfer_datatype(double *Lat,
                       double *Lon,
                       double *Elv,
                       float *height,
                       float *width)
{
    *Lat = (*Lat * 10000000.0);
    *Lon = (*Lon * 10000000.0);
    *Elv = (*Elv * 10.0);
    //*height = (*height * 100.0);
    //*width = (*width * 100.0);
}
double distance(double lat1, double lon1, double lat2, double lon2)
{
    // printf("lat1: %.14lf lon1: %.14lf lat2 %.14lf lon2 %.14lf\n", lat1, lon1,
    // lat2, lon2);
    double dLat = deg2rad(fabs(lat2 - lat1));
    double dLon = deg2rad(fabs(lon2 - lon1));
    double a = pow(sin(dLat / 2), 2) +
               cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * pow(sin(dLon / 2), 2);
    double c = 2 * asin(sqrt(a));
    int r = 6371;  // 6371 Radius of earth in kilometers. Use 3956 for miles
    return c * r * 1000;
}
double bearing(double lat, double lon, double lat2, double lon2)
{
    double teta1 = deg2rad(lat);
    double teta2 = deg2rad(lat2);
    double delta1 = deg2rad(lat2 - lat);
    double delta2 = deg2rad(lon2 - lon);

    //==================Heading Formula Calculation================//

    double y = sin(delta2) * cos(teta2);
    double x = cos(teta1) * sin(teta2) - sin(teta1) * cos(teta2) * cos(delta2);
    double brng = atan2(y, x);
    brng = rad2deg(brng);  // radians to degrees
    brng = (((int) brng + 360) % 360);

    return brng;
}
double deg2rad(double deg)
{
    return deg * (PI / 180);
}
double rad2deg(double rad)
{
    return rad * (180 / PI);
}