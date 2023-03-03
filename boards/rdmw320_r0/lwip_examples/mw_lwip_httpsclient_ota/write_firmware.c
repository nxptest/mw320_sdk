/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2021 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "write_firmware.h"

static const char srv_crt[] = SRV_CRT_RSA_SHA256_PEM;
const unsigned char *certificate_buffer;
unsigned long certificate_buffer_size;
size_t content_length;
TLSDataParams tlsDataParams;
typedef int (*data_fetch)(unsigned char *buf, size_t len);
struct partition_entry *part_get_passive_partition(struct partition_entry *p1, struct partition_entry *p2);

/* Send function used by mbedtls ssl */
static int lwipSend(void *fd, unsigned char const *buf, size_t len)
{
    return lwip_send((*(int *)fd), buf, len, 0);
}

/* Send function used by mbedtls ssl */
static int lwipRecv(void *fd, unsigned char const *buf, size_t len)
{
    return lwip_recv((*(int *)fd), (void *)buf, len, 0);
}

static uint32_t calculate_image_crc(uint32_t flash_addr, uint32_t size)
{
    int32_t result;
    uint32_t buf[32];
    uint32_t addr = flash_addr;
    uint32_t crc  = 0U;

    LOGD("Calculate image CRC");
    for (addr = flash_addr; addr < flash_addr + size - sizeof(buf); addr += sizeof(buf))
    {
        result = mflash_drv_read(addr, buf, sizeof(buf));
        if (result != WM_SUCCESS)
        {
            assert(false);
        }
        crc = soft_crc32(buf, sizeof(buf), crc);
    }

    /* Remaining data */
    result = mflash_drv_read(addr, buf, flash_addr + size - addr);
    if (result != WM_SUCCESS)
    {
        assert(false);
    }
    crc = soft_crc32(buf, flash_addr + size - addr, crc);

    return crc;
}

struct partition_entry *get_passive_firmware(void)
{
    short history = 0;
    struct partition_entry *f1, *f2;

    f1 = part_get_layout_by_id(FC_COMP_FW, &history);
    f2 = part_get_layout_by_id(FC_COMP_FW, &history);

    if (f1 == NULL || f2 == NULL)
    {
        LOGE("Unable to retrieve flash layout");
        return NULL;
    }

    return (struct partition_entry *)part_get_passive_partition(f1, f2);
}

void purge_stream_bytes(data_fetch data_fetch_cb, size_t length, unsigned char *scratch_buf, const int scratch_buflen)
{
    int size_read, to_read;
    while (length > 0)
    {
        to_read   = length > scratch_buflen ? scratch_buflen : length;
        size_read = data_fetch_cb(scratch_buf, to_read);
        if (size_read <= 0)
        {
            LOGE("Unexpected size returned");
            break;
        }
        length -= size_read;
    }
}

int fw_update_begin(struct partition_entry *p)
{
    /* Erase the passive partition before updating it */
    if (mflash_drv_erase(p->start, p->size))
    {
        LOGE("Failed to erase partition");
        return -WM_FAIL;
    }

    return WM_SUCCESS;
}

int fw_update_data(struct partition_entry *p, const char *data, size_t datalen)
{
    int32_t result;

    result = mflash_drv_write(p->start, (uint32_t *)data, datalen);
    if (result != WM_SUCCESS)
    {
        LOGE("Failed to write partition");
        return -WM_FAIL;
    }

    p->start += datalen;

    return WM_SUCCESS;
}

int verify_load_firmware(uint32_t flash_addr, uint32_t size)
{
    struct img_hdr ih;
    struct seg_hdr sh;
    int32_t result;

    if (size < (sizeof(ih) + sizeof(sh)))
    {
        return -WM_FAIL;
    }

    result = mflash_drv_read(flash_addr, (uint32_t *)&ih, sizeof(ih));
    if (result != WM_SUCCESS)
    {
        assert(false);
    }

    /* MCUXpresso SDK image has only 1 segment */
    if ((ih.magic_str != FW_MAGIC_STR) || (ih.magic_sig != FW_MAGIC_SIG) || ih.seg_cnt != 1U)
    {
        return -WM_FAIL;
    }

    result = mflash_drv_read(flash_addr + sizeof(ih), (uint32_t *)&sh, sizeof(sh));
    if (result != kStatus_Success)
    {
        assert(false);
    }

    /* Image size should just cover segment end. */
    if (sh.len + sh.offset != size)
    {
        return -WM_FAIL;
    }

    if (calculate_image_crc(flash_addr + sh.offset, sh.len) != sh.crc)
    {
        return -WM_FAIL;
    }

    return WM_SUCCESS;
}

int update_and_validate_firmware_image(data_fetch data_fetch_cb, size_t firmware_length, struct partition_entry *f)
{
    uint32_t filesize, read_length;
    int32_t size_read = 0;
    int error;
    struct img_hdr ih;
    int hdr_size;
    unsigned char buf[FIRMWARE_UPDATE_BUF_SIZE];
    uint32_t p_start;
    img_sec_hdr ish;

    LOGD("Updating firmware at address 0x%0x", f->start);
    LOGD("@ address 0x%x len: %d", f->start, f->size);

    /* Reserve partition start address for further reference */
    p_start = f->start;

    if (firmware_length == 0)
    {
        LOGE("Firmware binary zero length");
        return -WM_FAIL;
    }

    /* Verify firmware length first before proceeding further */
    if (firmware_length > f->size)
    {
        LOGE("Firmware binary too large %d > %d", firmware_length, f->size);
        purge_stream_bytes(data_fetch_cb, firmware_length, buf, FIRMWARE_UPDATE_BUF_SIZE);
        return -WM_FAIL;
    }

    /* Before erasing anything in flash, let's first get the firmware data buffer in
     * order to validate it.
     */

    hdr_size = (sizeof(ih) > sizeof(ish)) ? sizeof(ih) : sizeof(ish);
    size_read = 0;

    while ((hdr_size - size_read) != 0)
    {
        error = data_fetch_cb(buf + size_read, sizeof(ih) - size_read);
        if (error < 0)
        {
            LOGE("Failed to get firmware data for initial validation");
            error = -WM_FAIL;
            goto out;
        }
        size_read += error;
    }

    memcpy(&ish, buf, sizeof(ish));
    if (ish.magic == SEC_FW_MAGIC_SIG)
    {
        goto begin;
    }

    memcpy(&ih, buf, sizeof(ih));
    if (ih.magic_str != FW_MAGIC_STR || ih.magic_sig != FW_MAGIC_SIG)
    {
        LOGE("File data is not a firmware file");
        error = -WM_FAIL;
        purge_stream_bytes(data_fetch_cb, firmware_length - size_read, buf, FIRMWARE_UPDATE_BUF_SIZE);
        goto out;
    }

begin:
    error = fw_update_begin(f);
    if (error == -WM_FAIL)
    {
        LOGE("Failed to erase firmware area");
        purge_stream_bytes(data_fetch_cb, firmware_length - size_read, buf, FIRMWARE_UPDATE_BUF_SIZE);
        goto out;
    }

    filesize    = 0;
    read_length = FIRMWARE_UPDATE_BUF_SIZE;
    while (filesize < firmware_length)
    {
        if (filesize != 0)
        {
            /* we got the data the first time around */
            if ((firmware_length - filesize) < sizeof(buf))
                read_length = firmware_length - filesize;
            size_read = data_fetch_cb(buf, read_length);
        }
        if (size_read <= 0)
            break;
        error = fw_update_data(f, (char *)buf, size_read);
        if (error)
        {
            LOGE("Failed to write firmware data");
            goto out;
        }
        filesize += size_read;
    }
    if (filesize != firmware_length)
    {
        LOGE("Invalid size of receievd file size");
        error = -WM_FAIL;
        goto out;
    }

    LOGD("Firmware verification start ... filesize = %d", filesize);

    /* Then validate firmware data in flash */
    if (ish.magic == SEC_FW_MAGIC_SIG)
    {
        error = verify_load_firmware((p_start + SEC_FW_SIG_LEN), (filesize - SEC_FW_SIG_LEN));
    }
    else
    {
        error = verify_load_firmware(p_start, filesize);
    }

    f->start = p_start;

    if (error)
    {
        LOGE("Firmware verification failed with code %d", error);
        goto out;
    }

    part_set_active_partition(f);
    return WM_SUCCESS;
out:
    return error;
}

void https_client_tls_release()
{
    close(tlsDataParams.fd);
    mbedtls_entropy_free(&(tlsDataParams.entropy));
    mbedtls_ssl_config_free(&(tlsDataParams.conf));
    mbedtls_hmac_drbg_free(&(tlsDataParams.hmac_drbg));
    mbedtls_x509_crt_free(&(tlsDataParams.cacert));
    mbedtls_ssl_free(&(tlsDataParams.ssl));
}

static int _iot_tls_verify_cert(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    char buf[HTTPS_MAX_BUF_SIZE];
    ((void)data);

    LOGD("Verify requested for (Depth %d):", depth);
    mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "\r\n", crt);
    LOGD("%s", buf);

    if ((*flags) == 0)
    {
        LOGD("This certificate has no flags");
    }
    else
    {
        PRINTF(buf, sizeof(buf), "  ! ", *flags);
        LOGD("%s\n", buf);
    }

    return WM_SUCCESS;
}

int write_request(const char *url, const char *server_name)
{
    /* Write the GET request */
    int ret = 0;
    unsigned char https_buf[HTTPS_MAX_BUF_SIZE];

    int len = sprintf((char *)https_buf, HTTPS_REQ_10_HOST_FORMAT(url, server_name));

    while ((ret = mbedtls_ssl_write(&(tlsDataParams.ssl), https_buf, len)) <= WM_SUCCESS)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            LOGE("failed\n  ! mbedtls_ssl_write returned %d", ret);
            goto exit;
        }
    }

    len = ret;

    return ret;

exit:
    https_client_tls_release();
    return -WM_FAIL;
}

size_t update_content_length(const char *buf)
{
    /*
     * Search for "Content-Length: "
     * (@todo: fix if fragmented data recieved for string "Content-Length: ")
     */
#define HTTP_HDR_CONTENT_LEN               "Content-Length: "
#define HTTP_HDR_CONTENT_LEN_LEN           16
#define HTTP_HDR_CONTENT_LEN_DIGIT_MAX_LEN 10
    char *scontent_len = (char *)strstr(buf, HTTP_HDR_CONTENT_LEN);
    if (scontent_len)
    {
        content_length = strtol(scontent_len + HTTP_HDR_CONTENT_LEN_LEN, NULL, HTTP_HDR_CONTENT_LEN_DIGIT_MAX_LEN);
        return content_length;
    }
    else
    {
        return 0;
    }
}

int read_request()
{
    int ret = 0;
    int len = 0;
    unsigned char https_buf[HTTPS_MAX_BUF_SIZE];
    LOGD(" < Read from server:");

    do
    {
        len = sizeof(https_buf) - 1;
        memset(https_buf, 0, sizeof(https_buf));
        ret = mbedtls_ssl_read(&(tlsDataParams.ssl), https_buf, len);
        if (ret < 0)
        {
            LOGE("failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
            goto exit;
        }

        if (ret == 0)
        {
            LOGE("\n\nEOF\n\n");
            break;
        }
        if ((update_content_length((const char *)https_buf) > 0))
        {
            break;
        }
    } while (1);

    return ret;

exit:
    https_client_tls_release();
    return -WM_FAIL;
}

static int firmware_data_fetch_func(unsigned char *buf, size_t max_len)
{
    int ret = mbedtls_ssl_read(&(tlsDataParams.ssl), buf, max_len);

    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        return -WM_FAIL;

    if (ret < 0)
    {
        LOGE("failed\n  ! mbedtls_ssl_read returned %d Line:%d", ret, __LINE__);
        return -WM_FAIL;
    }

    if (ret == WM_SUCCESS)
    {
        LOGD("\n\nEOF\n\n");
        return WM_SUCCESS;
    }

    return ret;
}

static int client_mode_update_fw(char *url)
{
    char buf[512];
    int ret;
    const mbedtls_md_info_t *md_info;
    const char *pers = "https client";

    certificate_buffer      = (const unsigned char *)srv_crt;
    certificate_buffer_size = sizeof(srv_crt);

    mbedtls_ssl_init(&(tlsDataParams.ssl));
    mbedtls_x509_crt_init(&(tlsDataParams.cacert));

    mbedtls_hmac_drbg_init(&(tlsDataParams.hmac_drbg));

    LOGD("Seeding the random number generator");
    mbedtls_ssl_config_init(&(tlsDataParams.conf));

    mbedtls_entropy_init(&(tlsDataParams.entropy));

    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    if ((ret = mbedtls_hmac_drbg_seed(&(tlsDataParams.hmac_drbg), md_info, mbedtls_entropy_func,
                                      &(tlsDataParams.entropy), (const unsigned char *)pers, strlen(pers))) !=
        WM_SUCCESS)
    {
        LOGE(" failed\n  ! mbedtls_hmac_drbg_seed returned -0x%x", -ret);
        return -WM_FAIL;
    }

    LOGD("Loading the CA root certificate...");
    ret = mbedtls_x509_crt_parse(&(tlsDataParams.cacert), certificate_buffer, certificate_buffer_size);
    if (ret < 0)
    {
        LOGE("mbedtls_x509_crt_parse error");
        return ret;
    }

    LOGD("ok");
    LOGD("Connecting to %s/%s", SERVER, PORT);

    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    ret = getaddrinfo(SERVER, PORT, &hints, &res);
    if ((ret != WM_SUCCESS) || (res == NULL))
    {
        LOGE("Error: Net unknown Host");
        return ret;
    }

    tlsDataParams.fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (tlsDataParams.fd < 0)
    {
        LOGE("Error: Net socket failed");
        return -WM_FAIL;
    }

    ret = connect(tlsDataParams.fd, res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);

    if (ret != WM_SUCCESS)
    {
        close(tlsDataParams.fd);
        LOGE("Error: Net socket connect failed");
        return -WM_FAIL;
    }

    LOGD("Setting up the SSL/TLS structure...");
    if ((ret = mbedtls_ssl_config_defaults(&(tlsDataParams.conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != WM_SUCCESS)
    {
        LOGE(" failed\n  ! mbedtls_ssl_config_defaults returned -0x%x", -ret);
        return ret;
    }

    mbedtls_ssl_conf_verify(&(tlsDataParams.conf), _iot_tls_verify_cert, NULL);
    mbedtls_ssl_conf_authmode(&(tlsDataParams.conf), MBEDTLS_SSL_VERIFY_REQUIRED);

    mbedtls_ssl_conf_rng(&(tlsDataParams.conf), mbedtls_hmac_drbg_random, &(tlsDataParams.hmac_drbg));

    mbedtls_ssl_conf_ca_chain(&(tlsDataParams.conf), &(tlsDataParams.cacert), NULL);

    if ((ret = mbedtls_ssl_setup(&(tlsDataParams.ssl), &(tlsDataParams.conf))) != WM_SUCCESS)
    {
        LOGE(" failed\n  ! mbedtls_ssl_setup returned -0x%x", -ret);
        return ret;
    }

    if ((ret = mbedtls_ssl_set_hostname(&(tlsDataParams.ssl), SERVER)) != WM_SUCCESS)
    {
        LOGE(" failed\n  ! mbedtls_ssl_set_hostname returned %d", ret);
        return ret;
    }
    LOGD("SSL state connect : %d", tlsDataParams.ssl.state);

    mbedtls_ssl_set_bio(&(tlsDataParams.ssl), &(tlsDataParams.fd), lwipSend, (mbedtls_ssl_recv_t *)lwipRecv, NULL);

    LOGD("ok");
    LOGD("SSL state connect : %d", tlsDataParams.ssl.state);
    LOGD("Performing the SSL/TLS handshake...");

    while ((ret = mbedtls_ssl_handshake(&(tlsDataParams.ssl))) != WM_SUCCESS)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            LOGE(" failed\n  ! mbedtls_ssl_handshake returned -0x%x", -ret);
            if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
            {
                LOGE(
                    "    Unable to verify the server's certificate. "
                    "    Alternatively, you may want to use "
                    "auth_mode=optional for testing purposes.");
            }
            return ret;
        }
    }

    LOGD(" ok\r\n[ Protocol is %s ]\r\n[ Ciphersuite is %s ]\r\n", mbedtls_ssl_get_version(&(tlsDataParams.ssl)),
         mbedtls_ssl_get_ciphersuite(&(tlsDataParams.ssl)));

    if ((ret = mbedtls_ssl_get_record_expansion(&(tlsDataParams.ssl))) >= 0)
    {
        LOGD("[ Record expansion is %d ]", ret);
    }
    else
    {
        LOGE("[ Record expansion is unknown (compression) ]");
    }

    LOGD("Verifying peer X.509 certificate...");

    if ((tlsDataParams.flags = mbedtls_ssl_get_verify_result(&(tlsDataParams.ssl))) != WM_SUCCESS)
    {
        LOGE("failed");
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", tlsDataParams.flags);
        LOGE("%s", buf);
        ret = -WM_FAIL;
        https_client_tls_release();
        return ret;
    }
    else
    {
        LOGD("ok");
    }

    mbedtls_ssl_conf_read_timeout(&(tlsDataParams.conf), 10);

    write_request(url, SERVER);

    read_request();

    crc32_init();
    mflash_drv_init();

    struct partition_entry *p = get_passive_firmware();

    ret = update_and_validate_firmware_image(firmware_data_fetch_func, content_length, p);
    if (ret == WM_SUCCESS)
        LOGD("mcufw firmware update succeeded");
    else
        LOGE("Firmware update failed");

    https_client_tls_release();
    return ret;
}

void task_fw_update(void *param)
{
    int ret = 0;

    LOGD("mcufw update from url: %s", fw_url);
    ret = client_mode_update_fw(fw_url);
    if (ret == WM_SUCCESS)
    {
        LOGD("Rebooting the board ...\r\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        NVIC_SystemReset();
    }
    else
    {
        vTaskDelete(NULL);
    }
}

static void fw_update_handler(int argc, char **argv)
{
    BaseType_t result = 0;

    if (strlen(argv[1]) > MAX_URL_BUF_SIZE)
    {
        LOGE("URL length too large");
        return;
    }

    memset(fw_url, 0, sizeof(fw_url));
    strncpy(fw_url, argv[1], strlen(argv[1]));

    result =
        xTaskCreate(task_fw_update, "fw_update", TASK_FW_UPDATE_STACK_SIZE, task_fw_update_stack, TASK_FW_PRIO, &task_fw_update_handler);
    if (pdPASS != result)
    {
        assert(false);
    }
}

static struct cli_command fw_update_cmds[] = {
    {"fw_update", "<fw_url>", fw_update_handler},
};

int fw_update_cli_init(void)
{
    int i;

    for (i = 0; i < sizeof(fw_update_cmds) / sizeof(struct cli_command); i++)
        if (cli_register_command(&fw_update_cmds[i]))
            return -WM_FAIL;

    return WM_SUCCESS;
}
