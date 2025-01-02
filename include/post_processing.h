#ifndef POSTPROCESSING_H
#define POSTPROCESSING_H


#define PI 3.14159265358979323846

void transfer_datatype(double *Lat,
                       double *Lon,
                       double *elev,
                       float *height,
                       float *width);
double distance(double lat1, double lon1, double lat2, double lon2);
double bearing(double lat, double lon, double lat2, double lon2);
double deg2rad(double deg);
double rad2deg(double rad);
#endif