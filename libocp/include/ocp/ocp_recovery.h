/* SPDX-License-Identifier: Apache-2.0 */

/*
 * libocp - OCP Security WG "Secure Firmware Recovery" protocol (v1.0),
 * indirect (CMS memory window) image push over I2C/SMBus block transfers.
 *
 * Each SMBus transaction is:
 *   write: [command, byte count, payload ...]      (single i2c_msg)
 *   read : [command] write + I2C_M_RD read message (one I2C_RDWR ioctl);
 *          the response starts with the SMBus block byte count.
 *
 * All functions return 0 on success or a negative errno:
 *   -EINVAL   bad arguments
 *   -EIO      transport failure
 *   -EPROTO   malformed or unexpected device response
 *   -ENOTSUP  device only supports the v1.1 INDIRECT_FIFO mechanism
 *   -ETIMEDOUT polling exhausted (blocking convenience layer only)
 *
 * The single-transaction primitives never sleep; callers own all polling
 * delays. The blocking convenience layer (ocp_write_image/ocp_recover)
 * sleeps internally and must not be used inside an event loop.
 */

#ifndef LIBOCP_OCP_RECOVERY_H
#define LIBOCP_OCP_RECOVERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__GNUC__) || defined(__clang__)
#define OCP_NODISCARD __attribute__((warn_unused_result))
#else
#define OCP_NODISCARD
#endif

/* Recovery command codes (OCP Secure Firmware Recovery 1.0, table 8) */
enum ocp_cmd
{
    OCP_CMD_PROT_CAP = 0x22,
    OCP_CMD_DEVICE_ID = 0x23,
    OCP_CMD_DEVICE_STATUS = 0x24,
    OCP_CMD_DEVICE_RESET = 0x25,
    OCP_CMD_RECOVERY_CTRL = 0x26,
    OCP_CMD_RECOVERY_STATUS = 0x27,
    OCP_CMD_HW_STATUS = 0x28,
    OCP_CMD_INDIRECT_CTRL = 0x29,
    OCP_CMD_INDIRECT_STATUS = 0x2A,
    OCP_CMD_INDIRECT_DATA = 0x2B,
    OCP_CMD_VENDOR = 0x2C,
};

/* PROT_CAP capability bits */
#define OCP_PROT_CAP_INDIRECT_CTRL (1u << 5)
#define OCP_PROT_CAP_INDIRECT_FIFO (1u << 12)

/* DEVICE_STATUS status codes */
#define OCP_DEVICE_STATUS_PENDING 0x00
#define OCP_DEVICE_STATUS_HEALTHY 0x01
#define OCP_DEVICE_STATUS_ERROR 0x02
#define OCP_DEVICE_STATUS_RECOVERY_MODE 0x03
#define OCP_DEVICE_STATUS_RECOVERY_PENDING 0x04
#define OCP_DEVICE_STATUS_BOOT_FAILURE 0x0E
#define OCP_DEVICE_STATUS_FATAL_ERROR 0x0F

/* RECOVERY_STATUS status codes (low nibble) */
#define OCP_RECOVERY_STATUS_NOT_IN_RECOVERY 0x00
#define OCP_RECOVERY_STATUS_AWAITING_IMAGE 0x01
#define OCP_RECOVERY_STATUS_BOOTING_IMAGE 0x02
#define OCP_RECOVERY_STATUS_SUCCESS 0x03
#define OCP_RECOVERY_STATUS_FAILED 0x0C
#define OCP_RECOVERY_STATUS_AUTH_ERROR 0x0D
#define OCP_RECOVERY_STATUS_ENTERED_RECOVERY 0x0E
#define OCP_RECOVERY_STATUS_INVALID_CMS 0x0F

/* RECOVERY_CTRL recovery image selection */
#define OCP_RECOVERY_IMAGE_NONE 0x00
#define OCP_RECOVERY_IMAGE_FROM_CMS 0x01
#define OCP_RECOVERY_IMAGE_STORED 0x02

/* RECOVERY_CTRL activation */
#define OCP_RECOVERY_NO_ACTIVATE 0x00
#define OCP_RECOVERY_ACTIVATE 0x0F

/* Largest INDIRECT_DATA payload per SMBus block write */
#define OCP_CHUNK_SIZE_MAX 252u

#define OCP_CMS_DEFAULT 0u

/* Poll budgets used by the blocking layer; exported so event-loop callers
 * can implement the same policy with their own sleep primitive. */
/* Devices stall write acknowledgement while consuming a just-activated
 * image; successful writes acknowledge on the first poll, so a long
 * interval only costs time when the device is actually busy. */
#define OCP_ACK_POLL_RETRIES 5u
#define OCP_ACK_POLL_INTERVAL_MS 1000u
#define OCP_CHUNK_WRITE_RETRIES 3u
#define OCP_STATUS_POLL_RETRIES 30u
#define OCP_STATUS_POLL_INTERVAL_MS 1000u

/*
 * Transport hook: one bus transaction. wbuf/wlen is the write message
 * (always present); rlen > 0 requests a repeated-start read of rlen bytes
 * into rbuf. Returns 0 or negative errno.
 */
typedef int (*ocp_transfer_fn)(void* user, const uint8_t* wbuf, size_t wlen,
                               uint8_t* rbuf, size_t rlen);

struct ocp_ctx
{
    int fd;              /* /dev/i2c-N fd; -1 with a custom transport */
    uint16_t addr;       /* 7-bit device address */
    size_t chunk_size;   /* INDIRECT_DATA payload size, <= OCP_CHUNK_SIZE_MAX */
    uint8_t cms;         /* component memory space index */
    ocp_transfer_fn transfer;
    void* transfer_user;
};

struct ocp_prot_cap
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
OCP_NODISCARD int ocp_open(struct ocp_ctx* ctx, int bus, uint16_t addr);

/* Initialize ctx with a caller-provided transport (no fd is opened). */
OCP_NODISCARD int ocp_init_transport(struct ocp_ctx* ctx, ocp_transfer_fn fn,
                                     void* user, uint16_t addr);

void ocp_close(struct ocp_ctx* ctx);

/* Single-transaction primitives (no sleeping) */

OCP_NODISCARD int ocp_get_prot_cap(struct ocp_ctx* ctx,
                                   struct ocp_prot_cap* out);

/* 0 if the v1.0 indirect image push can be attempted; -ENOTSUP if the
 * device advertises only the v1.1 INDIRECT_FIFO mechanism. */
OCP_NODISCARD int ocp_check_caps(const struct ocp_prot_cap* cap);

OCP_NODISCARD int ocp_get_device_id(struct ocp_ctx* ctx, uint8_t* buf,
                                    size_t buflen, size_t* outlen);

OCP_NODISCARD int ocp_get_device_status(struct ocp_ctx* ctx, uint8_t* status,
                                        uint8_t* protocol_error,
                                        uint16_t* reason_code);

/* status receives the low-nibble recovery status code; image_index the
 * high nibble; vendor_status the vendor byte. Out params may be NULL. */
OCP_NODISCARD int ocp_get_recovery_status(struct ocp_ctx* ctx, uint8_t* status,
                                          uint8_t* image_index,
                                          uint8_t* vendor_status);

/* DEVICE_RESET: reset the device into recovery mode with interface
 * mastering enabled ([0x01, 0x0F, 0x01]). */
OCP_NODISCARD int ocp_force_recovery(struct ocp_ctx* ctx);

OCP_NODISCARD int ocp_recovery_ctrl(struct ocp_ctx* ctx, uint8_t cms,
                                    uint8_t image_sel, uint8_t activate);

OCP_NODISCARD int ocp_indirect_ctrl(struct ocp_ctx* ctx, uint8_t cms,
                                    uint32_t offset);

/* ack receives the write-acknowledge bit (byte 1, bit 2); size the CMS
 * size in 4-byte units. Out params may be NULL. */
OCP_NODISCARD int ocp_indirect_status(struct ocp_ctx* ctx, uint8_t* status,
                                      bool* ack, uint32_t* size);

OCP_NODISCARD int ocp_indirect_data_write(struct ocp_ctx* ctx,
                                          const uint8_t* data, size_t len);

/* Block-read up to buflen bytes from the current indirect window (set
 * with INDIRECT_CTRL), e.g. to retrieve CMS log regions. outlen
 * receives the number of bytes the device returned (0 when drained). */
OCP_NODISCARD int ocp_indirect_data_read(struct ocp_ctx* ctx, uint8_t* buf,
                                         size_t buflen, size_t* outlen);

/* Blocking convenience layer (sleeps with nanosleep; for tests and
 * standalone tooling, NOT for use inside an event loop) */

typedef void (*ocp_progress_fn)(void* user, size_t written, size_t total);

/* Stream image into the currently configured CMS window via chunked
 * INDIRECT_DATA writes with per-chunk acknowledge polling. */
OCP_NODISCARD int ocp_write_image(struct ocp_ctx* ctx, const uint8_t* image,
                                  size_t size, ocp_progress_fn cb, void* user);

/* Full recovery sequence: PROT_CAP (tolerant) -> force recovery mode if
 * needed -> select CMS image -> stream image -> activate -> poll for
 * success. */
OCP_NODISCARD int ocp_recover(struct ocp_ctx* ctx, const uint8_t* image,
                              size_t size, ocp_progress_fn cb, void* user);

#ifdef __cplusplus
}
#endif

#endif /* LIBOCP_OCP_RECOVERY_H */
