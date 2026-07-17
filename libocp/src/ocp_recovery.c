/* SPDX-License-Identifier: Apache-2.0 */

#define _POSIX_C_SOURCE 200809L

#include "ocp/ocp_recovery.h"

#include <errno.h>
#include <string.h>
#include <time.h>

/* SMBus block responses lead with the byte count */
#define OCP_RECOV_RESP_LEN_PROT_CAP 16u
#define OCP_RECOV_RESP_LEN_DEVICE_ID 25u
#define OCP_RECOV_RESP_LEN_DEVICE_STATUS 25u
#define OCP_RECOV_RESP_LEN_RECOVERY_STATUS 3u
#define OCP_RECOV_RESP_LEN_INDIRECT_STATUS 7u

#define OCP_RECOV_INDIRECT_STATUS_ACK_MASK 0x04u

static const char ocp_magic[8] = {'O', 'C', 'P', ' ', 'R', 'E', 'C', 'V'};

static int ocp_cmd_write(struct ocp_recov_ctx* ctx, uint8_t cmd,
                         const uint8_t* payload, size_t len)
{
    uint8_t buf[2 + OCP_RECOV_CHUNK_SIZE_MAX];

    if (ctx == NULL || ctx->transfer == NULL ||
        len > OCP_RECOV_CHUNK_SIZE_MAX || (len > 0 && payload == NULL))
    {
        return -EINVAL;
    }

    buf[0] = cmd;
    buf[1] = (uint8_t)len;
    if (len > 0)
    {
        memcpy(&buf[2], payload, len);
    }

    return ctx->transfer(ctx->transfer_user, buf, len + 2, NULL, 0);
}

static int ocp_cmd_read(struct ocp_recov_ctx* ctx, uint8_t cmd, uint8_t* rbuf,
                        size_t rlen)
{
    uint8_t wbuf[1];
    int rc;

    if (ctx == NULL || ctx->transfer == NULL || rbuf == NULL || rlen < 2)
    {
        return -EINVAL;
    }

    wbuf[0] = cmd;

    rc = ctx->transfer(ctx->transfer_user, wbuf, sizeof(wbuf), rbuf, rlen);
    if (rc < 0)
    {
        return rc;
    }

    /* rbuf[0] is the SMBus block byte count of the payload that follows */
    if (rbuf[0] == 0 || (size_t)rbuf[0] > rlen - 1)
    {
        return -EPROTO;
    }

    return 0;
}

int ocp_recov_get_prot_cap(struct ocp_recov_ctx* ctx,
                           struct ocp_recov_prot_cap* out)
{
    uint8_t resp[OCP_RECOV_RESP_LEN_PROT_CAP];
    int rc;

    if (out == NULL)
    {
        return -EINVAL;
    }

    rc = ocp_cmd_read(ctx, OCP_RECOV_CMD_PROT_CAP, resp, sizeof(resp));
    if (rc < 0)
    {
        return rc;
    }

    if (resp[0] < 15 || memcmp(&resp[1], ocp_magic, sizeof(ocp_magic)) != 0)
    {
        return -EPROTO;
    }

    memcpy(out->magic, &resp[1], sizeof(ocp_magic));
    out->magic[sizeof(ocp_magic)] = '\0';
    out->major = resp[9];
    out->minor = resp[10];
    out->caps = (uint16_t)(resp[11] | ((uint16_t)resp[12] << 8));
    out->num_cms = resp[13];
    out->max_resp_time = resp[14];
    out->heartbeat_period = resp[15];

    return 0;
}

int ocp_recov_check_caps(const struct ocp_recov_prot_cap* cap)
{
    if (cap == NULL)
    {
        return -EINVAL;
    }

    /* Only reject when the device positively advertises FIFO-only
     * support; devices commonly leave the capability bits unset. */
    if ((cap->caps & OCP_RECOV_PROT_CAP_INDIRECT_FIFO) != 0 &&
        (cap->caps & OCP_RECOV_PROT_CAP_INDIRECT_CTRL) == 0)
    {
        return -ENOTSUP; /* TODO: v1.1 INDIRECT_FIFO path */
    }

    return 0;
}

int ocp_recov_get_device_id(struct ocp_recov_ctx* ctx, uint8_t* buf,
                            size_t buflen, size_t* outlen)
{
    uint8_t resp[OCP_RECOV_RESP_LEN_DEVICE_ID];
    size_t len;
    int rc;

    if (buf == NULL || buflen == 0)
    {
        return -EINVAL;
    }

    rc = ocp_cmd_read(ctx, OCP_RECOV_CMD_DEVICE_ID, resp, sizeof(resp));
    if (rc < 0)
    {
        return rc;
    }

    len = resp[0];
    if (len > buflen)
    {
        len = buflen;
    }
    memcpy(buf, &resp[1], len);

    if (outlen != NULL)
    {
        *outlen = len;
    }

    return 0;
}

int ocp_recov_get_device_status(struct ocp_recov_ctx* ctx,
                                enum ocp_recov_device_status* status,
                                uint8_t* protocol_error, uint16_t* reason_code)
{
    uint8_t resp[OCP_RECOV_RESP_LEN_DEVICE_STATUS];
    int rc;

    rc = ocp_cmd_read(ctx, OCP_RECOV_CMD_DEVICE_STATUS, resp, sizeof(resp));
    if (rc < 0)
    {
        return rc;
    }

    if (resp[0] < 4)
    {
        return -EPROTO;
    }

    if (status != NULL)
    {
        *status = (enum ocp_recov_device_status)resp[1];
    }
    if (protocol_error != NULL)
    {
        *protocol_error = resp[2];
    }
    if (reason_code != NULL)
    {
        *reason_code = (uint16_t)(resp[3] | ((uint16_t)resp[4] << 8));
    }

    return 0;
}

int ocp_recov_get_recovery_status(struct ocp_recov_ctx* ctx,
                                  enum ocp_recov_recovery_status* status,
                                  uint8_t* image_index, uint8_t* vendor_status)
{
    uint8_t resp[OCP_RECOV_RESP_LEN_RECOVERY_STATUS];
    int rc;

    rc = ocp_cmd_read(ctx, OCP_RECOV_CMD_RECOVERY_STATUS, resp, sizeof(resp));
    if (rc < 0)
    {
        return rc;
    }

    if (status != NULL)
    {
        *status = (enum ocp_recov_recovery_status)(resp[1] & 0x0F);
    }
    if (image_index != NULL)
    {
        *image_index = (resp[1] >> 4) & 0x0F;
    }
    if (vendor_status != NULL)
    {
        *vendor_status = (resp[0] >= 2) ? resp[2] : 0;
    }

    return 0;
}

int ocp_recov_force_recovery(struct ocp_recov_ctx* ctx)
{
    /* device reset, enter recovery on reset, enable interface mastering */
    const uint8_t payload[3] = {0x01, 0x0F, 0x01};

    return ocp_cmd_write(ctx, OCP_RECOV_CMD_DEVICE_RESET, payload,
                         sizeof(payload));
}

int ocp_recov_recovery_ctrl(struct ocp_recov_ctx* ctx, uint8_t cms,
                            enum ocp_recov_image_sel image_sel,
                            enum ocp_recov_activation activate)
{
    const uint8_t payload[3] = {cms, (uint8_t)image_sel, (uint8_t)activate};

    return ocp_cmd_write(ctx, OCP_RECOV_CMD_RECOVERY_CTRL, payload,
                         sizeof(payload));
}

int ocp_recov_indirect_ctrl(struct ocp_recov_ctx* ctx, uint8_t cms,
                            uint32_t offset)
{
    const uint8_t payload[6] = {
        cms,
        0x00, /* reserved */
        (uint8_t)(offset & 0xFF),
        (uint8_t)((offset >> 8) & 0xFF),
        (uint8_t)((offset >> 16) & 0xFF),
        (uint8_t)((offset >> 24) & 0xFF),
    };

    return ocp_cmd_write(ctx, OCP_RECOV_CMD_INDIRECT_CTRL, payload,
                         sizeof(payload));
}

int ocp_recov_indirect_status(struct ocp_recov_ctx* ctx, uint8_t* status,
                              bool* ack, uint32_t* size)
{
    uint8_t resp[OCP_RECOV_RESP_LEN_INDIRECT_STATUS];
    int rc;

    rc = ocp_cmd_read(ctx, OCP_RECOV_CMD_INDIRECT_STATUS, resp, sizeof(resp));
    if (rc < 0)
    {
        return rc;
    }

    if (status != NULL)
    {
        *status = resp[1];
    }
    if (ack != NULL)
    {
        *ack = (resp[1] & OCP_RECOV_INDIRECT_STATUS_ACK_MASK) != 0;
    }
    if (size != NULL)
    {
        if (resp[0] < 6)
        {
            return -EPROTO;
        }
        *size = (uint32_t)resp[3] | ((uint32_t)resp[4] << 8) |
                ((uint32_t)resp[5] << 16) | ((uint32_t)resp[6] << 24);
    }

    return 0;
}

int ocp_recov_indirect_data_write(struct ocp_recov_ctx* ctx,
                                  const uint8_t* data, size_t len)
{
    if (ctx == NULL || data == NULL || len == 0 || len > ctx->chunk_size ||
        ctx->chunk_size > OCP_RECOV_CHUNK_SIZE_MAX)
    {
        return -EINVAL;
    }

    return ocp_cmd_write(ctx, OCP_RECOV_CMD_INDIRECT_DATA, data, len);
}

int ocp_recov_indirect_data_read(struct ocp_recov_ctx* ctx, uint8_t* buf,
                                 size_t buflen, size_t* outlen)
{
    uint8_t resp[1 + 255];
    size_t want;
    size_t len;
    int rc;

    if (buf == NULL || buflen == 0)
    {
        return -EINVAL;
    }

    want = (buflen > 255) ? 255 : buflen;

    rc = ocp_cmd_read(ctx, OCP_RECOV_CMD_INDIRECT_DATA, resp, want + 1);
    if (rc < 0)
    {
        return rc;
    }

    len = resp[0];
    if (len > want)
    {
        len = want;
    }
    memcpy(buf, &resp[1], len);

    if (outlen != NULL)
    {
        *outlen = len;
    }

    return 0;
}

/* Blocking convenience layer */

static void ocp_msleep(unsigned int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int ocp_wait_write_ack(struct ocp_recov_ctx* ctx)
{
    unsigned int poll;
    bool ack = false;
    int rc;

    for (poll = 0; poll < OCP_RECOV_ACK_POLL_RETRIES; poll++)
    {
        rc = ocp_recov_indirect_status(ctx, NULL, &ack, NULL);
        if (rc < 0)
        {
            return rc;
        }
        if (ack)
        {
            return 0;
        }
        ocp_msleep(OCP_RECOV_ACK_POLL_INTERVAL_MS);
    }

    return -ETIMEDOUT;
}

int ocp_recov_write_image(struct ocp_recov_ctx* ctx, const uint8_t* image,
                          size_t size, ocp_recov_progress_fn cb, void* user)
{
    size_t offset = 0;

    if (ctx == NULL || image == NULL || size == 0 || ctx->chunk_size == 0 ||
        ctx->chunk_size > OCP_RECOV_CHUNK_SIZE_MAX)
    {
        return -EINVAL;
    }

    while (offset < size)
    {
        size_t len = size - offset;
        unsigned int attempt;
        int rc = -ETIMEDOUT;

        if (len > ctx->chunk_size)
        {
            len = ctx->chunk_size;
        }

        for (attempt = 0; attempt < OCP_RECOV_CHUNK_WRITE_RETRIES; attempt++)
        {
            rc = ocp_recov_indirect_data_write(ctx, &image[offset], len);
            if (rc < 0)
            {
                return rc;
            }

            rc = ocp_wait_write_ack(ctx);
            if (rc == 0)
            {
                break;
            }
            if (rc != -ETIMEDOUT)
            {
                return rc;
            }
        }
        if (rc < 0)
        {
            return rc;
        }

        offset += len;

        if (cb != NULL)
        {
            cb(user, offset, size);
        }
    }

    return 0;
}

int ocp_recov_recover(struct ocp_recov_ctx* ctx, const uint8_t* image,
                      size_t size, ocp_recov_progress_fn cb, void* user)
{
    struct ocp_recov_prot_cap cap;
    enum ocp_recov_device_status dev_status = OCP_RECOV_DEVICE_STATUS_PENDING;
    enum ocp_recov_recovery_status rec_status =
        OCP_RECOV_RECOVERY_STATUS_NOT_IN_RECOVERY;
    unsigned int poll;
    int rc;

    if (ctx == NULL || image == NULL || size == 0)
    {
        return -EINVAL;
    }

    /* Tolerant: many devices do not implement PROT_CAP; only fail when
     * the device positively reports FIFO-only support. */
    rc = ocp_recov_get_prot_cap(ctx, &cap);
    if (rc == 0)
    {
        rc = ocp_recov_check_caps(&cap);
        if (rc < 0)
        {
            return rc;
        }
    }

    rc = ocp_recov_get_device_status(ctx, &dev_status, NULL, NULL);
    if (rc < 0)
    {
        return rc;
    }

    if (dev_status != OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE)
    {
        rc = ocp_recov_force_recovery(ctx);
        if (rc < 0)
        {
            return rc;
        }

        for (poll = 0; poll < OCP_RECOV_STATUS_POLL_RETRIES; poll++)
        {
            ocp_msleep(OCP_RECOV_STATUS_POLL_INTERVAL_MS);

            rc = ocp_recov_get_device_status(ctx, &dev_status, NULL, NULL);
            if (rc == 0 && dev_status == OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE)
            {
                break;
            }
        }
        if (dev_status != OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE)
        {
            return -ETIMEDOUT;
        }
    }

    rc = ocp_recov_recovery_ctrl(ctx, ctx->cms,
                                 OCP_RECOV_RECOVERY_IMAGE_FROM_CMS,
                                 OCP_RECOV_RECOVERY_NO_ACTIVATE);
    if (rc < 0)
    {
        return rc;
    }

    rc = ocp_recov_indirect_ctrl(ctx, ctx->cms, 0);
    if (rc < 0)
    {
        return rc;
    }

    rc = ocp_recov_write_image(ctx, image, size, cb, user);
    if (rc < 0)
    {
        return rc;
    }

    rc = ocp_recov_recovery_ctrl(ctx, ctx->cms,
                                 OCP_RECOV_RECOVERY_IMAGE_FROM_CMS,
                                 OCP_RECOV_RECOVERY_ACTIVATE);
    if (rc < 0)
    {
        return rc;
    }

    for (poll = 0; poll < OCP_RECOV_STATUS_POLL_RETRIES; poll++)
    {
        rc = ocp_recov_get_recovery_status(ctx, &rec_status, NULL, NULL);
        if (rc == 0)
        {
            if (rec_status == OCP_RECOV_RECOVERY_STATUS_SUCCESS)
            {
                return 0;
            }
            if (rec_status >= OCP_RECOV_RECOVERY_STATUS_FAILED)
            {
                return -EIO;
            }
        }
        ocp_msleep(OCP_RECOV_STATUS_POLL_INTERVAL_MS);
    }

    return -ETIMEDOUT;
}
