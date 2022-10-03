/* 
    * **************************************************************** *
    * This header file including define, enum, struct and API will be  *
    * replaced with "us_caster_service.h" in subsequent version of SDK *
    * **************************************************************** *
*/

#ifndef __DSRC_CASTER_SERVICE_H__
#define __DSRC_CASTER_SERVICE_H__

/**
*   @defgroup dsrc_caster_service
*   @{
*   This section introduces the dsrc caster service APIs including terms and acronyms, dsrc caster service functions.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "common_caster_service.h"

#define DSRC_MAX_SSP (31U)
#define DSRC_CASTER_PKT_SIZE_MAX (1500U)

/**
 * @brief This structure defines DSRC TX information
 */
typedef struct dsrc_tx_info {
    uint8_t data_rate; /**< this field is used if data_rate_is_present is true */
    int8_t tx_power_level; /**< this field is used if tx_power_level_is_present is true */
    uint8_t user_priority; /**< this field is used if user_priority_is_present is true */
    uint64_t expiry_time; /**< this field is used if expiry_time_is_present is true */
    uint8_t ssp[DSRC_MAX_SSP]; /**< Unused now */
    uint8_t ssp_len; /**< Unused now */
    uint8_t srcmac[6];
    bool data_rate_is_present; /**< if true, the field of data_rate is active */
    bool tx_power_level_is_present; /**< if true, the field of tx_power_level is active */
    bool user_priority_is_present; /**< if true, the field of user_priority is active */
    bool expiry_time_is_present; /**< if true, the field of expiry_time is active */
    bool ssp_is_present; /**< Unused now */
    bool srcmac_is_present;
} dsrc_tx_info_t;

/**
 * @brief This enum defines DSRC security decapsulation result
 */
typedef enum {
    DSRC_DECAP_NONE = 0, /**< Non-execution security decapulation */
    DSRC_DECAP_INVALID_FMT, /**< Invalid EtsiTs103097 format */
    DSRC_DECAP_VERIFIED_PKT, /**< Valid EtsiTs103097 SignedData (verification success) */
    DSRC_DECAP_UNVERIFIABLE_PKT, /**< Valid EtsiTs103097 UnsecuredData (unable to verify) OR SignedData (verification failure) */
} dsrc_decap_status_e;

/**
 * @brief This structure defines DSRC RX security information
 */
typedef struct dsrc_rx_security {
    dsrc_decap_status_e status; /**< Decapsulation result */
    uint8_t ssp_len; /**< Number of Service Specific Permission (SSP) */
    uint8_t ssp[MAX_SSP]; /**< Service Specific Permission (SSP) */
} dsrc_rx_security_t;

/**
 * @brief This structure defines DSRC RX information
 */
typedef struct dsrc_rx_info {
    struct timeval timestamp; /**< The received time of the packet from lower layer */
    uint8_t channel_used; /**< The real channel which the packet is received on */
    int8_t rssi; /**< The receving RSSI of the packet */
    uint8_t channel_number; /**< parsed from the packet header, extension field */
    int8_t tx_power; /**< parsed from the packet header, extension field */
    uint8_t data_rate; /**< parsed from the packet header, extension field */
    dsrc_rx_security_t security;
    uint8_t srcmac[6];
    bool channel_number_is_present; /**< if 1, the field of channel_number is active */
    bool tx_power_is_present; /**< if 1, the field of tx_power is active */
    bool data_rate_is_present; /**< if 1, the field of data_rate is active */
} dsrc_rx_info_t;

/**
*   @brief Init context of a caster
*
*   @returns The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int dsrc_caster_init(void);

/**
*   @brief Create an instance of a caster and set up the resources needed for the caster
*
*   @param [in] config indicates the communication setting, including IP, caster id and caster communication mode.
*               IP format example: "127.0.0.1".
*               caster_id: ID in Caster Profile defined in the config file.
*               caster_comm_mode: TX or RX.
*   @param [out] caster_handler indicates the handler of a caster to be created
*   @return The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int dsrc_caster_create(caster_handler_t *caster_handler, caster_comm_config_t *config);

/**
*   @brief Transmit a DSRC message
*
*   @param [in] caster_handler indicates a created caster handler.
*   @param [in] *tx_info indicates to replace the default configurations with this tx_info.
*               if NULL, using TX setting from JSON config, otherwise using TX setting with this tx_info parameters.
*   @param [in] buf indicates the message to be sent.
*   @param [in] len indicates the number of bytes to be sent
*   @return The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int dsrc_caster_tx(caster_handler_t caster_handler, dsrc_tx_info_t *tx_info, uint8_t *buf, size_t len);

/**
*   @brief Receive a DSRC message
*
*   @param [in] caster_handler indicates a created caster handler.
*   @param [out] *rx_info is used to receive the information of rx_info.
*   @param [out] buf is used to receive a message.
*   @param [out] len indicates the number of bytes received.
*   @return The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int dsrc_caster_rx(caster_handler_t caster_handler, dsrc_rx_info_t *rx_info, uint8_t *buf, size_t buf_size, size_t *len);

int dsrc_caster_subscribe(caster_handler_t caster_handler, caster_endpoint_t *p_endpoint);
int dsrc_caster_unsubscribe(caster_handler_t caster_handler, caster_endpoint_t *p_endpoint);

/**
*   @brief Release a caster instance and clean up the resources needed for the caster instance
*
*   @param [in] caster_handler indicates the caster handler to be released.
*   @return The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int dsrc_caster_release(caster_handler_t caster_handler);

/**
*   @brief Get thread configuration for application usage
*
*   @param [out] dsrc_app_thread_info_t is use to set tx and rx message thread configuration in application.
*   @returns none
 */
void dsrc_app_thread_config_get(app_thread_config_t *thread_config);

/**
*   @brief Deinit context of a caster
*
*   @returns The status of the API invoked, Please refer to RETURN VALUES for details.
*/
int dsrc_caster_deinit(void);
/**
 *  @}
 */
#endif
