#ifndef GPS_INFORMATION_H
#define GPS_INFORMATION_H

#define PI 3.14159265358979323846

double get_distance(double lat1, double lon1, double lat2, double lon2);
int get_bearing(double lat1, double lon1, double lat2, double lon2);

#endif  /* GPS_INFORMATION_H */