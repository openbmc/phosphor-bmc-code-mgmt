/* SPDX-License-Identifier: Apache-2.0 */

/*
 * ocp_fw_recovery - OCP Security WG "Secure Firmware Recovery" protocol
 * (v1.0), indirect (CMS memory window) image push over I2C/SMBus block
 * transfers.
 *
 * Each SMBus transaction is:
 *   write: [command, byte count, payload ...]      (single i2c_msg)
 *   read : [command] write + I2C_M_RD read message (one I2C_RDWR ioctl);
 *          the response starts with the SMBus block byte count.
 *
 * Return-code convention (uniform across every int-returning function in
 * this header - there is no per-function variation): 0 on success, or one
 * of the following negative errno values on failure. The specific value
 * identifies the failure class:
 *   -EINVAL   bad arguments
 *   -EIO      transport failure
 *   -EPROTO   malformed or unexpected device response
 *   -ENOTSUP  device only supports the v1.1 INDIRECT_FIFO mechanism
 *   -ETIMEDOUT polling exhausted (blocking convenience layer only)
 * All such functions are marked OCP_RECOV_NODISCARD so the caller cannot
 * accidentally drop the status.
 *
 * The single-transaction primitives never sleep; callers own all polling
 * delays. The blocking convenience layer
 * (ocp_recov_write_image/ocp_recov_recover) sleeps internally and must not be
 * used inside an event loop.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__GNUC__) || defined(__clang__)
#define OCP_RECOV_NODISCARD __attribute__((warn_unused_result))
#else
#define OCP_RECOV_NODISCARD
#endif

/* Recovery command codes (OCP Secure Firmware Recovery 1.0, table 8) */
enum ocp_recov_cmd
{
    OCP_RECOV_CMD_PROT_CAP = 0x22,
    OCP_RECOV_CMD_DEVICE_ID = 0x23,
    OCP_RECOV_CMD_DEVICE_STATUS = 0x24,
    OCP_RECOV_CMD_DEVICE_RESET = 0x25,
    OCP_RECOV_CMD_RECOVERY_CTRL = 0x26,
    OCP_RECOV_CMD_RECOVERY_STATUS = 0x27,
    OCP_RECOV_CMD_HW_STATUS = 0x28,
    OCP_RECOV_CMD_INDIRECT_CTRL = 0x29,
    OCP_RECOV_CMD_INDIRECT_STATUS = 0x2A,
    OCP_RECOV_CMD_INDIRECT_DATA = 0x2B,
    OCP_RECOV_CMD_VENDOR = 0x2C,
};

/* PROT_CAP capability bits */
#define OCP_RECOV_PROT_CAP_INDIRECT_CTRL (1U << 5)
#define OCP_RECOV_PROT_CAP_INDIRECT_FIFO (1U << 12)

/*
 * Protocol code sets. Each set is a distinct C enum so the overlapping
 * numeric spaces cannot be silently mixed up (e.g. 0x03 is "recovery
 * mode" as a device status but "success" as a recovery status).
 * Reserved or vendor-defined wire values outside the enumerations are
 * passed through unchanged.
 */

/* DEVICE_STATUS status codes */
enum ocp_recov_device_status
{
    OCP_RECOV_DEVICE_STATUS_PENDING = 0x00,
    OCP_RECOV_DEVICE_STATUS_HEALTHY = 0x01,
    OCP_RECOV_DEVICE_STATUS_ERROR = 0x02,
    OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE = 0x03,
    OCP_RECOV_DEVICE_STATUS_RECOVERY_PENDING = 0x04,
    OCP_RECOV_DEVICE_STATUS_RUNNING_RECOVERY = 0x05,
    OCP_RECOV_DEVICE_STATUS_BOOT_FAILURE = 0x0E,
    OCP_RECOV_DEVICE_STATUS_FATAL_ERROR = 0x0F,
};

/* RECOVERY_STATUS status codes (low nibble) */
enum ocp_recov_recovery_status
{
    OCP_RECOV_RECOVERY_STATUS_NOT_IN_RECOVERY = 0x00,
    OCP_RECOV_RECOVERY_STATUS_AWAITING_IMAGE = 0x01,
    OCP_RECOV_RECOVERY_STATUS_BOOTING_IMAGE = 0x02,
    OCP_RECOV_RECOVERY_STATUS_SUCCESS = 0x03,
    OCP_RECOV_RECOVERY_STATUS_FAILED = 0x0C,
    OCP_RECOV_RECOVERY_STATUS_AUTH_ERROR = 0x0D,
    OCP_RECOV_RECOVERY_STATUS_ENTERED_RECOVERY = 0x0E,
    OCP_RECOV_RECOVERY_STATUS_INVALID_CMS = 0x0F,
};

/* RECOVERY_CTRL recovery image selection */
enum ocp_recov_image_sel
{
    OCP_RECOV_RECOVERY_IMAGE_NONE = 0x00,
    OCP_RECOV_RECOVERY_IMAGE_FROM_CMS = 0x01,
    OCP_RECOV_RECOVERY_IMAGE_STORED = 0x02,
};

/* RECOVERY_CTRL activation */
enum ocp_recov_activation
{
    OCP_RECOV_RECOVERY_NO_ACTIVATE = 0x00,
    OCP_RECOV_RECOVERY_ACTIVATE = 0x0F,
};

/* Largest INDIRECT_DATA payload per SMBus block write */
#define OCP_RECOV_CHUNK_SIZE_MAX 252U

#define OCP_RECOV_CMS_DEFAULT 0U

/* Poll budgets used by the blocking layer; exported so event-loop callers
 * can implement the same policy with their own sleep primitive. */
/* Devices stall write acknowledgement while consuming a just-activated
 * image; successful writes acknowledge on the first poll, so a long
 * interval only costs time when the device is actually busy. */
#define OCP_RECOV_ACK_POLL_RETRIES 5U
#define OCP_RECOV_ACK_POLL_INTERVAL_MS 1000U
#define OCP_RECOV_CHUNK_WRITE_RETRIES 3U
#define OCP_RECOV_STATUS_POLL_RETRIES 30U
#define OCP_RECOV_STATUS_POLL_INTERVAL_MS 1000U

/*
 * Transport hook: one bus transaction. wbuf/wlen is the write message
 * (always present); rlen > 0 requests a repeated-start read of rlen bytes
 * into rbuf. Returns 0 or negative errno.
 */
typedef int (*ocp_recov_transfer_fn)(void* user, const uint8_t* wbuf,
                                     size_t wlen, uint8_t* rbuf, size_t rlen);

struct ocp_recov_ctx
{
    int fd;            /* /dev/i2c-N fd; -1 with a custom transport */
    uint16_t addr;     /* 7-bit device address */
    size_t chunk_size; /* INDIRECT_DATA payload size, <=
                          OCP_RECOV_CHUNK_SIZE_MAX */
    uint8_t cms;       /* component memory space index */
    ocp_recov_transfer_fn transfer;
    void* transfer_user;
};

struct ocp_recov_prot_cap
{
    char magic[9]; /* "OCP RECV", NUL-terminated */
    uint8_t major;
    uint8_t minor;
    uint16_t caps;
    uint8_t num_cms;
    uint8_t max_resp_time;    /* 2^n us */
    uint8_t heartbeat_period; /* 2^n us */
};

/* Open /dev/i2c-<bus> and initialize ctx with the built-in ioctl transport
 * and default chunk_size/cms. */
OCP_RECOV_NODISCARD int ocp_recov_open(struct ocp_recov_ctx* ctx, uint16_t bus,
                                       uint16_t addr);

/* Initialize ctx with a caller-provided transport (no fd is opened). */
OCP_RECOV_NODISCARD int ocp_recov_init_transport(struct ocp_recov_ctx* ctx,
                                                 ocp_recov_transfer_fn fn,
                                                 void* user, uint16_t addr);

void ocp_recov_close(struct ocp_recov_ctx* ctx);

/* Single-transaction primitives (no sleeping) */

OCP_RECOV_NODISCARD int ocp_recov_get_prot_cap(struct ocp_recov_ctx* ctx,
                                               struct ocp_recov_prot_cap* out);

/* 0 if the v1.0 indirect image push can be attempted; -ENOTSUP if the
 * device advertises only the v1.1 INDIRECT_FIFO mechanism. */
OCP_RECOV_NODISCARD int ocp_recov_check_caps(
    const struct ocp_recov_prot_cap* cap);

OCP_RECOV_NODISCARD int ocp_recov_get_device_id(
    struct ocp_recov_ctx* ctx, uint8_t* buf, size_t buflen, size_t* outlen);

OCP_RECOV_NODISCARD int ocp_recov_get_device_status(
    struct ocp_recov_ctx* ctx, enum ocp_recov_device_status* status,
    uint8_t* protocol_error, uint16_t* reason_code);

/* status receives the low-nibble recovery status code; image_index the
 * high nibble; vendor_status the vendor byte. Out params may be NULL. */
OCP_RECOV_NODISCARD int ocp_recov_get_recovery_status(
    struct ocp_recov_ctx* ctx, enum ocp_recov_recovery_status* status,
    uint8_t* image_index, uint8_t* vendor_status);

/* DEVICE_RESET: reset the device into recovery mode with interface
 * mastering enabled ([0x01, 0x0F, 0x01]). */
OCP_RECOV_NODISCARD int ocp_recov_force_recovery(struct ocp_recov_ctx* ctx);

OCP_RECOV_NODISCARD int ocp_recov_recovery_ctrl(
    struct ocp_recov_ctx* ctx, uint8_t cms, enum ocp_recov_image_sel image_sel,
    enum ocp_recov_activation activate);

OCP_RECOV_NODISCARD int ocp_recov_indirect_ctrl(struct ocp_recov_ctx* ctx,
                                                uint8_t cms, uint32_t offset);

/* ack receives the write-acknowledge bit (byte 1, bit 2); size the CMS
 * size in 4-byte units. Out params may be NULL. */
OCP_RECOV_NODISCARD int ocp_recov_indirect_status(
    struct ocp_recov_ctx* ctx, uint8_t* status, bool* ack, uint32_t* size);

OCP_RECOV_NODISCARD int ocp_recov_indirect_data_write(
    struct ocp_recov_ctx* ctx, const uint8_t* data, size_t len);

/* Block-read up to buflen bytes from the current indirect window (set
 * with INDIRECT_CTRL), e.g. to retrieve CMS log regions. outlen
 * receives the number of bytes the device returned (0 when drained). */
OCP_RECOV_NODISCARD int ocp_recov_indirect_data_read(
    struct ocp_recov_ctx* ctx, uint8_t* buf, size_t buflen, size_t* outlen);

/* Blocking convenience layer (sleeps with nanosleep; for tests and
 * standalone tooling, NOT for use inside an event loop) */

typedef void (*ocp_recov_progress_fn)(void* user, size_t written, size_t total);

/* Stream image into the currently configured CMS window via chunked
 * INDIRECT_DATA writes with per-chunk acknowledge polling. */
OCP_RECOV_NODISCARD int ocp_recov_write_image(
    struct ocp_recov_ctx* ctx, const uint8_t* image, size_t size,
    ocp_recov_progress_fn cb, void* user);

/* Full recovery sequence: PROT_CAP (tolerant) -> force recovery mode if
 * needed -> select CMS image -> stream image -> activate -> poll for
 * success. */
OCP_RECOV_NODISCARD int ocp_recov_recover(struct ocp_recov_ctx* ctx,
                                          const uint8_t* image, size_t size,
                                          ocp_recov_progress_fn cb, void* user);

#ifdef __cplusplus
}
#endif
