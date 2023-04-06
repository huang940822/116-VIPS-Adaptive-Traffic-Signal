#ifndef SPM_REPEAT_H
#define SPM_REPEAT_H

#include <stdbool.h>

void SPM_repeater_start(bool send_flag);
void *SPM_repeater();
void sigintHandlerSPM(int sig_num);
#endif