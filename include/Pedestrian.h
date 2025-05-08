#ifndef PEDESTRIANOBJECT_H
#define PEDESTRIANOBJECT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Pedestrian {
    uint8_t PERSON_ID;
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
    uint8_t count;
    uint8_t camera_no;
} PedestrianList;


#endif
