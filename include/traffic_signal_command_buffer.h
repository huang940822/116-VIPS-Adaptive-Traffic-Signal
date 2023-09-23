#include "typedefine.h"

#define SUBPHASEID_NUM 8
#define CYCLE_NUM 2

// resume 是 app 要回復原本時治狀態下的指令
// app 可以自己計算需要先償還多少再交給補償機制補償
#define RESUME_ID "RESUME"
#define RESUME_ID_DONE "RESUME_DONE"

extern tsc_command_object_t command_buf[CYCLE_NUM][SUBPHASEID_NUM];
extern pthread_mutex_t mutex_command_buf;
extern uint8_t cycle_index;

void command_buf_init();
void command_buf_clear();
void command_buf_polling();
int command_buf_insert_effect_time(tsc_command_t *command);
int command_buf_insert_adjustment(tsc_command_t *command);
void command_buf_print();
bool check_command_buf_empty();
uint8_t is_in_conpensation();

/* Return codes of command buf insert */
typedef enum command_buf_err {
    INSERT_ACCEPT = 0,
    INVALID_APP_ID = -1,
    INVALID_APP_PRIORITY = -2,
    INVALID_TARGET_PHASE = -3,
    INVALID_CYCLE = -4,
    INVALID_PHASE = -5,
    INVALID_EFFECT_TIME = -6,
    INVALID_ADJUSTMENT = -7,
    INVALID_HOST_OBU_NAME = -8,
    IMPROPER_PRIORITY = -9,
    IMPROPER_EFFECT_TIME = -10,
    INCOMP_TSPDONOTHING = -11,
} command_buf_err_t;