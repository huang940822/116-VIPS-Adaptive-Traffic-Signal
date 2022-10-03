#ifndef __COMMON_CASTER_SERVICE_H__
#define __COMMON_CASTER_SERVICE_H__

/**
*   @defgroup common_caster_service
*   @{
*   This section introduces the common caster service APIs including terms and acronyms, common caster service functions.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

#define MAX_SSP (32U)
#define MAC_ADDR_SIZE 6
#define CASTER_PKT_SIZE_MAX (1500U)
#define INVALID_CASTER_HANDLER UINT8_MAX

typedef uint8_t caster_handler_t;

/**
 * @brief This enum defines caster communication mode
 */
typedef enum {
    CASTER_MODE_TX = 0, /**< Transimission mode */
    CASTER_MODE_RX, /**< Reception mode */
    CASTER_MODE_SPLMNT, /**< Supplementary mode */
    CASTER_MODE_POTI, /**< POTI mode */
    CASTER_MODE_MAX /**< Numbers of modes */
} caster_comm_mode_e;

typedef enum {
    CASTER_FORMAT_DEFAULT = 0,
    CASTER_FORMAT_RAWDATA,
    CASTER_FORMAT_XML,
    CASTER_FORMAT_MAX
} caster_format_e;

typedef enum {
    CASTER_ENDPOINT_ID_TYPE_CASTER_ID = 0,
    CASTER_ENDPOINT_ID_TYPE_MAX
} caster_endpoint_id_type_e;

/**
 * @brief This structure defines caster communication configuration
 */
typedef struct {
    const char *ip; /**< IP */
    uint16_t caster_id; /**< Caster ID */
    caster_comm_mode_e caster_comm_mode; /**< Caster communication mode */
} caster_comm_config_t;

/**
 * @brief This structure defines caster endpoint configuration
 */
typedef struct {
    const char *ip; /**< IP */
    caster_endpoint_id_type_e type; /**< Caster endpoint ID type */
    union {
        uint16_t caster_id; /**< Caster ID */
    } id;
} caster_endpoint_t;

/**
 * @brief This structure defines TX and RX thread configuration for application
 */
typedef struct app_thread_config {
    int32_t tx_thread_priority_high;
    int32_t tx_thread_priority_low;
    char *tx_thread_name;
    int32_t rx_thread_priority_high;
    int32_t rx_thread_priority_low;
    char *rx_thread_name;
} app_thread_config_t;

typedef struct caster_thread_info {
    int32_t tx_thread_priority_high;
    int32_t tx_thread_priority_low;
    char *tx_thread_name;
    int32_t rx_thread_priority_high;
    int32_t rx_thread_priority_low;
    char *rx_thread_name;
} caster_thread_info_t;

typedef enum {
    SPLMNT_INFO_TYPE_CASTER_FORMAT_GET,
    SPLMNT_INFO_TYPE_COEXISTENCE_MODE_SET,
    SPLMNT_INFO_TYPE_COEXISTENCE_MODE_GET
} splmnt_info_type_e;

typedef struct caster_format_get_req {
    uint8_t dummy;
} caster_format_get_req_t;

typedef struct caster_format_get_rsp {
    caster_format_e caster_format;
} caster_format_get_rsp_t;

typedef struct coexistence_mode_set_req {
    bool is_enabled;
} coexistence_mode_set_req_t;

typedef struct coexistence_mode_set_rsp {
    int32_t rc;
} coexistence_mode_set_rsp_t;

typedef struct coexistence_mode_get_req {
    uint8_t dummy;
} coexistence_mode_get_req_t;

typedef struct coexistence_mode_get_rsp {
    bool is_enabled;
} coexistence_mode_get_rsp_t;

/**
 * @brief This structure defines ITS-G5 SPLMNT REQ information
 */
typedef struct castert_splmnt_req_info {
    splmnt_info_type_e type;
    union {
        caster_format_get_req_t caster_format_get_req;
        coexistence_mode_set_req_t coexistence_mode_set_req;
        coexistence_mode_get_req_t coexistence_mode_get_req;
    } value;
} caster_splmnt_req_info_t;

/**
 * @brief This structure defines ITS-G5 SPLMNT RSP information
 */
typedef struct caster_splmnt_rsp_info {
    splmnt_info_type_e type;
    union {
        caster_format_get_rsp_t caster_format_get_rsp;
        coexistence_mode_set_rsp_t coexistence_mode_set_rsp;
        coexistence_mode_get_rsp_t coexistence_mode_get_rsp;
    } value;
} caster_splmnt_rsp_info_t;

#endif
