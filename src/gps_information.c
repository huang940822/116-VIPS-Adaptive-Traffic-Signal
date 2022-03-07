#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "gps_information.h"

/* This function converts decimal degrees to radians */
static double deg2rad(double deg)
{
    return (deg * PI / 180);
}

/* This function converts radians to decimal degrees */
static double rad2deg(double rad)
{
    return (rad * 180 / PI);
}

/* Haversine formula */
double get_distance(double lat1, double lon1, double lat2, double lon2)
{
    lat1 = deg2rad(lat1);
    lon1 = deg2rad(lon1);
    lat2 = deg2rad(lat2);
    lon2 = deg2rad(lon2);
    double R = 6371; /* earth’s mean radius = 6,371km */
    double lat = lat2 - lat1;
    double lon = lon2 - lon1;

    double a =
        pow(sin(lat / 2), 2) + cos(lat1) * cos(lat2) * pow(sin(lon / 2), 2);
    double c = 2 * asin(sqrt(a));
    double d = R * c * 1000;
    return d;
}

int get_bearing(double lat1, double lon1, double lat2, double lon2)
{
    lat1 = deg2rad(lat1);
    lon1 = deg2rad(lon1);
    lat2 = deg2rad(lat2);
    lon2 = deg2rad(lon2);

    double lon = lon2 - lon1;
    double a = log(tan(lat2 / 2.0 + PI / 4.0) / tan(lat1 / 2.0 + PI / 4.0));
    if (abs(lon) > PI) {
        if (lon > 0.0)
            lon = -(2.0 * PI - lon);
        else
            lon = (2.0 * PI + lon);
    }
    return (int) (rad2deg(atan2(lon, a)) + 360.0) % 360;
}