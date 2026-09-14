#ifndef PEDESTRIANOBJECT_H
#define PEDESTRIANOBJECT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Pedestrian {
    uint8_t PERSON_ID;
    uint8_t waiting;
    uint16_t cx;
    uint16_t cy;
    uint16_t location;
    uint8_t direction;
    double projection;
    struct Pedestrian *next;
    double velocity;
} Pedestrian;

typedef struct PedestrianList {
    Pedestrian *tab;
    uint32_t count;
    uint32_t camera_no;
    uint32_t device_num;
} PedestrianList;


#endif
