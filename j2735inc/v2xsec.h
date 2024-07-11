/*******************************************************************************
 * Copyright (C) Unex Technology Corporation - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 *
 *  !! please don't modify this file, it will cause software updated issues !!
 *
 *
 * @file    v2xsec.h
 * @brief   This file provides definition of struct, data type and APIs.
 * @author  Unex
 *******************************************************************************/
#ifndef __V2XSEC_H__
#define __V2XSEC_H__
#include "stdint.h"

/**
 * @brief This enum defines the decapsulated SPDU status
 */
typedef enum {
    V2XSEC_NONE = 0, /**< Non-execution security decapsulation (default value) */
    V2XSEC_INVALID_FMT, /**< Invalid (US: IEEE1609.2 / EU: EtsiTs103097) Format */
    V2XSEC_UNSECURED_PKT, /**< Valid (US: IEEE1609.2 / EU: EtsiTs103097) UnsecuredData */
    V2XSEC_VERIFIABLE_SIGNED_PKT, /**< Valid (US: IEEE1609.2 / EU: EtsiTs103097) SignedData (verification success) */
    V2XSEC_UNVERIFIABLE_SIGNED_PKT /**< Valid (US: IEEE1609.2 / EU: EtsiTs103097) SignedData (verification failure) */
} v2xsec_decap_status_e;

/**
 * @brief This enum defines the decap socket interface
 */
typedef enum {
    V2XSEC_DECAP_SOCKET_IF_0 = 0,
    V2XSEC_DECAP_SOCKET_IF_1,
    V2XSEC_DECAP_SOCKET_IF_2,
    V2XSEC_DECAP_SOCKET_IF_3,
    V2XSEC_DECAP_SOCKET_IF_4
} v2xsec_decap_socket_if_e;

/**
 * @brief This defines the signer id type
 */
typedef enum {
    DIGEST_TYPE = 0,
    CERTIFICATE_TYPE,
    SELF_TYPE
} signer_id_e;


/**
 * @brief This defines the result code of security
 */
typedef enum {
    V2XSEC_RES_SUCCESS = 0,
    V2XSEC_RES_INVALID_INPUT,
    V2XSEC_RES_SPDU_GEN_TIME_NOT_AVALIABLE,
    V2XSEC_RES_SPDU_GEN_LOC_NOT_AVALIABLE,
    V2XSEC_RES_VERIFICATION_FAIL
} v2xsec_res_info_e;

/**
 * @brief This structure defines V2X decapsulation info
 */
typedef struct {
    v2xsec_decap_status_e status;
    struct {
        uint64_t gen_time_us;
        uint64_t life_time_us;
    } spdu_info;
    struct {
        uint64_t next_crl_time;
    } cert_info;
    v2xsec_res_info_e rc_info;
} v2xsec_decap_info_t;
#endif