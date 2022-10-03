/* Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef __POTI_CASTER_SERVICE_H__
#define __POTI_CASTER_SERVICE_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "common_caster_service.h"

/**
 *  @brief This enum defines POTI GNSS satellite system type.
 */
typedef enum {
    POTI_SAT_TYPE_GPS = 0, /**< GPS */
    POTI_SAT_TYPE_GLONASS = 1, /**< GLONASS */
    POTI_SAT_TYPE_NUM /**< Number of POTI GNSS satellite system types */
} poti_satellite_type_t;

/**
 *  @brief This structure defines POTI service time.
 */
typedef struct poti_service_tm {
    uint16_t year; /**< year, to store the number in 4 decimal digits, such 2018 */
    uint16_t ms; /**< milliseconds, [0, 999] */
    uint8_t mon; /**< month, [1, 12] */
    uint8_t mday; /**< day in a month, [1, 31] */
    uint8_t hour; /**< hour, [0, 23] */
    uint8_t min; /**< minute, [0, 59] */
    uint8_t sec; /**< second, [0, 60] */
} poti_service_tm_t;

/**
 *  @brief This enum defines POTI system time sync status.
 */
typedef enum {
    POTI_SYSTIME_NOT_SYNC = 0, /**< system time not sync. */
    POTI_SYSTIME_SYNC_GNSS = 1, /**< system time sync. to GNSS */
} poti_systime_status_t;

/**
 * @brief This enum defines POTI antenna status.
 */
typedef enum {
    POTI_ANTENNA_STATUS_NA = 0, /**< No status */
    POTI_ANTENNA_STATUS_NORMAL = 1, /**< normal */
    POTI_ANTENNA_STATUS_BAD = 2, /**< open or short */
} poti_antenna_status_t;

/**
 *  @brief This structure defines POTI fix time.
 */
typedef struct poti_fix_time {
    poti_service_tm_t utc; /**< UTC of POTI fix time*/
    double tai_since_2004; /**< TAI of POTI fix time */
} poti_fix_time_t;

/**
 *  @brief This enum defines POTI fix mode.
 */
typedef enum {
    POTI_FIX_MODE_NA = 0, /**< Not available */
    POTI_FIX_MODE_NO_FIX = 1, /**< No fix */
    POTI_FIX_MODE_2D = 2, /**< 2D position fix */
    POTI_FIX_MODE_3D = 3, /**< 3D position fix */
    POTI_FIX_MODE_CFG = 4,
} poti_fix_mode_t;

/**
 *  @brief This enum defines POTI fix status.
 */
typedef enum {
    CASTER_FIX_STATUS_NA = 0, /**< No status */
    CASTER_FIX_STATUS_GNSS = 1, /**< fix from gnss */
    CASTER_FIX_STATUS_DGNSS = 2, /**< fix from gnss with dgnss or rtk */
    CASTER_FIX_STATUS_CONFIG = 3, /* get fix data from configuration file */
} poti_fix_status_t;

/* For C-V2X , POTI time sync state of access layer(ACL) */
typedef enum {
    CASTER_ACL_TIME_SYNC_LOSS = 0,
    CASTER_ACL_TIME_SYNC_SYNCING,
    CASTER_ACL_TIME_SYNC_SYNCED
} poti_acl_ts_state_t;

/**
 *  @brief This structure defines POTI fix data.
 */
typedef struct poti_fix_data {
    poti_fix_status_t status; /**< POTI fix status */
    poti_fix_mode_t mode; /**< POTI fix mode */
    poti_fix_time_t time; /**< POTI fix time */
    /* position */
    double latitude; /**< degree, [-90.0, 90.0] */
    double longitude; /**< degree, [-180.0, 180.0] */
    double altitude; /**< meter */

    /* heading, speed */
    double course_over_ground; /**< relative to true north (clockwise) in degree. [0.0, 360.0] */
    double horizontal_speed; /**< meter per second */
    double vertical_speed; /**< meter per second */

    /* Error */
    double err_time; /**< second */
    /* horizontal position standard deviation ellipse */
    double err_smajor_axis; /**< semi-major axis length in meter */
    double err_sminor_axis; /**< semi-minor axis length in meter */
    double err_smajor_orientation; /**< major axis direction relative to true norther (clockwise) in degree. [0.0, 360.0] */
    double err_altitude; /**< in meter */
    double err_course_over_ground; /**< degree */
    double err_horizontal_speed; /**< meter per second */
    double err_vertical_speed; /**< meter per second */

    int num_satellites_used; /**< Number of satellites used */
    int num_satellites_visible[POTI_SAT_TYPE_NUM]; /**< Number of visible satellites */
} poti_fix_data_t;

/**
 *  @brief This structure defines POTI GNSS information.
 */
typedef struct poti_gnss_info {
    poti_systime_status_t time_sync_status; /**< POTI time sync status */
    poti_acl_ts_state_t acl_time_sync_state; /**< For C-V2X , POTI time sync state of access layer(ACL) */
    uint32_t acl_unsync_times; /**< For C-V2X , Times of losing time sync */
    poti_fix_mode_t mode; /**< POTI fix mode */
    poti_antenna_status_t antenna_status; /**< POTI antenna status */

    /* skyview */
    poti_service_tm_t skyview_time; /**< POTI service time of skyview time */
    int num_satellites_used; /**< Number of satellites used */
    int num_satellites_visible[POTI_SAT_TYPE_NUM]; /**< Number of satellites used */
} poti_gnss_info_t;

/* APIs for users */
/**
*   @brief Receive a POTI fix data Call this API will wait until the POTI update fix data for making sure the fix data is the latest data.
*          The timeout is 3 seconds.
*          Basically, the wait time is less than 100ms while the GNSS output rate is 10Hz.
*
*   @param [in] poti_handler indicates the created poti caster handler for receiving POTI fix data.
*   @param [out] *poti_fix_data is used to receive the information of the POTI fix data. The pointer cannot be NULL.
*   @return The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int poti_caster_fix_data_get(caster_handler_t caster_handler, poti_fix_data_t *poti_fix_data);

/**
*   @brief Get the GNSS status In contrast to "poti_caster_fix_data_get" API, it will get current GNSS status immediately.
*
*   @param [in] poti_handler indicates the created poti caster handler for receiving GNSS info.
*   @param [out] *poti_gnss_info is used to receive the information of GNSS status.
*   @return The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int poti_caster_gnss_info_get(caster_handler_t caster_handler, poti_gnss_info_t *poti_gnss_info);

#endif
