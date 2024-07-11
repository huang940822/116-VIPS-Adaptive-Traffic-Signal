/*******************************************************************************
 * Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 *
 *  !! please don't modify this file, it will cause software updated issues !!
 *
 *
 * @file    v2xsec_service.h
 * @brief   This file provides definition of struct, data type and APIs.
 * @author  Unex
 *******************************************************************************/
#ifndef __V2XSEC_SERVICE_H__
#define __V2XSEC_SERVICE_H__
#include "stdint.h"
#include "stdbool.h"
#include "v2xsec.h"
/**
*   @addtogroup V2XSEC_SERVICE
*   @{
*/
#define V2XSEC_SSP_MAX_LEN 32

/**
 * @brief This structure defines config of V2X security encapsulation
 */
typedef struct {
    /** @name input parameter */
    bool is_enforced_unsecured_pkt_encap;
    struct {
        uint32_t psid; /**< PSID of Ieee1609Dot2Data */
        uint32_t input_msg_len; /**< input data length */
        uint8_t *p_input_msg; /**< input data */
        uint32_t ssp_len; /**< SSP length (Max length is 32 bytes)
                                    This field is for permission check.
                                    If this length is zero, it will by skip the SSP permission check.
                                    If this length isn't zero, it will check the ssp permssion of used certiface. */
        uint8_t ssp[V2XSEC_SSP_MAX_LEN]; /**< SSP permission content (only support bitmap)
                                    This field is for permission check.
                                    To check the used certificate has this permission or not.  */
        uint32_t sec_msg_buf_size; /**< sec_msg_buf_size specifies the buffer size of Ieee1609Dot2Data msg */
        bool is_ee_changeover_enabled; /**< en/disable EE certificate changeover.
                                    please note that it won't work, if number of certificate is less than 2 */
    } i;
    /** @name output parameter */
    struct {
        uint32_t sec_msg_len; /**< output length Ieee1609Dot2Data msg */
        uint8_t *p_sec_msg; /**< output Ieee1609Dot2Data msg */
    } o;
} v2xsec_encap_t;

/**
 * @brief This structure defines config of V2X security decapsulation
 */
typedef struct {
    /** @name input parameter */
    struct {
        v2xsec_decap_socket_if_e socket_if;
        uint32_t sec_msg_len; /**< SPDU length */
        uint8_t *p_sec_msg; /**< SPDU, it is supposed to be Ieee1609Dot2Data */
        uint32_t plaintext_msg_buf_size; /**< output data buffer size */
    } i;
    /** @name output parameter */
    struct {
        uint32_t ssp_len; /**< extracted SSP length (bytes) */
        uint8_t ssp[V2XSEC_SSP_MAX_LEN]; /**< extracted SSP.
                            If SPDU is Ieee1609Dot2 signed data and includes signer certificate,
                            it will extract SSP from the certificate. (only support bitmap type) */
        uint32_t plaintext_msg_len; /**< extracted length of raw data */
        uint8_t *p_plaintext_msg; /**< extracted raw data */
        uint32_t psid; /**< PSID of Ieee1609Dot2Data */
        v2xsec_decap_info_t info;
        uint8_t cert_hashid8[8];
        signer_id_e signer_id_type;
    } o;
} v2xsec_decap_t;

/**
 * @brief This enum defines security profile type
 *          mapping to modules/v2xcastd/inc/v2xcast_def.h: sec_profile_type_e
 *          mapping to modules/v2xcast_service/inc/v2xcast_service.h: security_state_t
 */
typedef enum {
    V2XSEC_PROFILE_NONE = -1,
    V2XSEC_PROFILE_UNSECURED_MSG, /**< xmit/recv SPDUs are supposed to be Ieee1609Dot2 unsecured data */
    V2XSEC_PROFILE_SIGNED_MSG, /**< xmit/recv SPDUs are supposed to be Ieee1609Dot2 signed data */
    V2XSEC_PROFILE_MAX_NUM /**< security profile numbers, it can't be used */
} v2xsec_profile_e;

/**
 * @brief This enum defines security config
 * User need to config PSIDs that are gonna to transmit/receive and corresponded security profiles during initialization
 */
typedef struct {
    uint32_t psid;
    v2xsec_profile_e security_profile;
} v2xsec_config_t;

/**
*   @brief This function initializes V2X Security.
*   Please note that it is supposed to be invoked before calling any V2X Security APIs.
*
*   @param[in] p_sdee_name indicates different user. It shall be a unique SDEE name. The length can't exceed 32 bytes.
*   @param[in] p_sec_cfg indicates the security configs.
*   Please note that PSID can't be set repetitively. One PSID only can have a corresponded security profile.
*   @param[in] cfg_num is certificate config number. The maximum number can't exceed 32.
*   @return SUCCESS(0) means success, others means fail.
*/
int32_t v2xsec_init(char *p_sdee_name,
                    v2xsec_config_t *p_sec_cfg,
                    uint32_t cfg_num);

/**
*   @brief This function de-initializes V2X Security.
*
*/
void v2xsec_deinit(void);

/**
*   @brief This function encapsulates msg into Ieee1609Dot2 msg.
*
*   @param[in] encap_info indicates encapsulation information.
*   @return SUCCESS(0) means success, others means fail.
*/
int32_t v2xsec_encap(v2xsec_encap_t *encap_info);

/**
*   @brief This function decapsulates Ieee1609Dot2 msg.
*   If the msg is Ieee1609Dot2 unsecured msg, it will extract raw msg directly.
*   If the msg is Ieee1609Dot2 signed msg, it will verify the msg additionally.
*
*   @param[in] decap_info indicates encapsulation information.
*   @return SUCCESS(0) means success, others means fail.
*/
int32_t v2xsec_decap(v2xsec_decap_t *decap_info);

/**
 *  @}
 */
#endif