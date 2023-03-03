/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <partition.h>
#include <crc32.h>
#include <wm_os.h>
#include <mflash_drv.h>
#include <wm_net.h>   
#include <cli.h>      
#include <ip_addr.h>    
#include <flash_layout.h>
#include <boot_flags.h>

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/hmac_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

/* Set MW_LWIP_HTTPSCLIENT_OTA_DEBUG 1 to enable debug logs */
#define MW_LWIP_HTTPSCLIENT_OTA_DEBUG 0
#define LOGD(fmt, ...) \
            do { if (MW_LWIP_HTTPSCLIENT_OTA_DEBUG) PRINTF(fmt "\n\r", ##__VA_ARGS__); } while (0)
#define LOGE(fmt, ...) PRINTF(fmt "\n\r", ##__VA_ARGS__)

#define FIRMWARE_UPDATE_BUF_SIZE    256
#define HTTPS_MAX_BUF_SIZE          1024
#define MAX_URL_BUF_SIZE            512
#define SEC_FW_SIG_LEN              284

#define FW_MAGIC_STR                (('M' << 0)|('R' << 8)|('V' << 16)|('L' << 24))
#define FW_MAGIC_SIG                ((0x7BUL << 0) | (0xF1UL << 8) | (0x9CUL << 16) | (0x2EUL << 24))
#define SEC_FW_MAGIC_SIG            (('S' << 0)|('B' << 8)|('F' << 16)|('H' << 24))

const char *SERVER = "192.168.1.2";
const char *PORT = "443";

#define HTTPS_REQ_10_HOST "GET %s HTTP/1.1\r\n" /* URL */\
    "Host: %s\r\n" /* server name */ \
    "User-Agent: MW320\r\n" /* User-Agent */ \
    "\r\n"

#define HTTPS_REQ_10_HOST_FORMAT(url, srv_name) HTTPS_REQ_10_HOST, url, srv_name

#define SRV_CRT_RSA_SHA256_PEM                                             \
    "-----BEGIN CERTIFICATE-----\r\n"                                      \
    "MIIC+DCCAeACCQCh7pf48cFdyzANBgkqhkiG9w0BAQsFADA+MQswCQYDVQQGEwJJ\r\n" \
    "TjELMAkGA1UECAwCTUgxDDAKBgNVBAoMA05YUDEUMBIGA1UEAwwLMTkyLjE2OC4x\r\n" \
    "LjIwHhcNMjEwMzE2MDczMDI4WhcNMjIwMzE2MDczMDI4WjA+MQswCQYDVQQGEwJJ\r\n" \
    "TjELMAkGA1UECAwCTUgxDDAKBgNVBAoMA05YUDEUMBIGA1UEAwwLMTkyLjE2OC4x\r\n" \
    "LjIwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDE0CxjdERtfOUvM6ZK\r\n" \
    "2EhcAskcZB3Mdi+4lEeS7PSe/AXEy3bG5RRFvPrTVgGigWu6vW+9mjWIHvpUHW66\r\n" \
    "t4DnqO4+1+cy1+c7YFSqAD1rZEyZfLEA23rasPONjyeDgWSPKd+X7mefzNutOaA5\r\n" \
    "qK4bqPXDjPN9iyQS61uwMFcuk2A5VZiTfNb+lGp5wvfR1HG1/LbTbNC4DQLIfXCw\r\n" \
    "ODc31gPZXS+EyzZ0SiOJpR2fvlUdmnmbdOFDSYB5Fsd9q93AHuCIzt+bPjAlClH4\r\n" \
    "tLbWvsXK3vANmnI5sAt0ImM3PvhKa2c1t2za8mADRWrAzW1ycheGM1wW3/9Wudd/\r\n" \
    "Y2kjAgMBAAEwDQYJKoZIhvcNAQELBQADggEBABjIOfUBKmMRbplAXJJG5DaWHIDM\r\n" \
    "5G17V316WamMQ9FJEej2ABiw98MO8v282IStnihq0wrJqO1yTt/taWeLCEtfo2jV\r\n" \
    "HMJuV9Mg+AOCH19Zi6gBZ62nHh/mzeZQugIqK9keAHEJ+kP0EBZTeoKgCr5avRiz\r\n" \
    "Aa4Nna+7UvlN96noW7R6yZRqBiGMlX08gbnj5Lk7yO/Q64FvFeEQz/5hr0oDB5rQ\r\n" \
    "ZKL7LLMjXejuR5K8cP/P2gCJZ5IJGxWJypN7Azeq+AxGJHPOqVUvGWPE35TU4EoT\r\n" \
    "7YkXz2S3hQjCnSl0Bl3igeyUcobMGHahvUg6sE5ZhzimalMGwp0qYZzCzs8=\r\n"     \
    "-----END CERTIFICATE-----\r\n"

/*
 * Firmware magic signature
 *
 * First word is the string "MRVL" and is endian invariant.
 * Second word = magic value 0x2e9cf17b.
 * Third word = time stamp (seconds since 00:00:00 UTC, January 1, 1970).
 */
struct img_hdr {
    uint32_t magic_str;
    uint32_t magic_sig;
    uint32_t time;
    uint32_t seg_cnt;
    uint32_t entry;
};

struct seg_hdr {
    uint32_t type;
    uint32_t offset;
    uint32_t len;
    uint32_t laddr;
    uint32_t crc;
};

struct tlv_hdr {
    uint32_t magic;
    uint32_t len;
    uint32_t crc;
};
typedef struct tlv_hdr img_sec_hdr;

typedef struct _TLSDataParams
{
    mbedtls_entropy_context entropy;
    mbedtls_hmac_drbg_context hmac_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    uint32_t flags;
    mbedtls_x509_crt cacert;
    /* lwIP socket file descriptor */
    int fd;

} TLSDataParams;

portSTACK_TYPE *task_fw_update_stack = NULL;
const int TASK_FW_PRIO       = OS_PRIO_3;
TaskHandle_t task_fw_update_handler;
char fw_url[MAX_URL_BUF_SIZE];
const int TASK_FW_UPDATE_STACK_SIZE = 4096;
