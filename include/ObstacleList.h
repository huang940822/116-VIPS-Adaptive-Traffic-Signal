#ifndef OBSTACLELIST_H
#define OBSTACLELIST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Obstacle {
    int32_t laneID;
    int32_t ObstacleID;
    double lat;  /* Latitude (-900000000..900000001) */
    double Long; /* Longitude (-1799999999..1800000001) */
    double elev; /* Elevation (-4096..61439) */
    float width;
    float length;
    int32_t heading;
    /*  LSB of 0.0125 degrees, A range of 0 to 359.9875 degrees*/
    int32_t description;  //車種
    int32_t speed;
    int32_t hour;
    int32_t minute;
    float second;
} Obstacle;

typedef struct ObstacleList {
    Obstacle *tab;
    int32_t dirct; // camera_num, 以最靠近北端為 0 ，依序遞增。 e.g. 路口為北、東北、南、西南，編號仍為 0, 1, 2, 3
    int32_t count;
    int32_t device_num;
} ObstacleList;

#endif