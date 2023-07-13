#ifndef MAP_UTILS_H
#define MAP_UTILS_H

#include <stdint.h>
#include "j2735_map.h"
#define ERR_MSG_SZ 128

void map_msg_init(MapData *map);
void map_msg_update(MapData *map);
void map_print(MapData *map);
void map_decode(uint8_t *rx_buf, int rx_buf_len);
void map_dump_mem(void *data, int len);
void map_signal_group(MapData *map);

#endif