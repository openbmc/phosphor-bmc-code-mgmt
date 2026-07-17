/* SPDX-License-Identifier: Apache-2.0 */

#include "ocp/ocp_recovery.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int ocp_i2c_transfer(void* user, const uint8_t* wbuf, size_t wlen,
                            uint8_t* rbuf, size_t rlen)
{
    struct ocp_recov_ctx* ctx = user;
    struct i2c_msg msgs[2] = {0};
    struct i2c_rdwr_ioctl_data xfer;

    if (ctx == NULL || ctx->fd < 0 || wbuf == NULL || wlen == 0 ||
        wlen > UINT16_MAX || rlen > UINT16_MAX || (rlen > 0 && rbuf == NULL))
    {
        return -EINVAL;
    }

    msgs[0].addr = ctx->addr;
    msgs[0].flags = 0;
    msgs[0].len = (uint16_t)wlen;
    /* The kernel API is not const-correct; the buffer is only read. */
    msgs[0].buf = (uint8_t*)wbuf;

    if (rlen > 0)
    {
        msgs[1].addr = ctx->addr;
        msgs[1].flags = I2C_M_RD;
        msgs[1].len = (uint16_t)rlen;
        msgs[1].buf = rbuf;
    }

    xfer.msgs = msgs;
    xfer.nmsgs = (rlen > 0) ? 2 : 1;

    if (ioctl(ctx->fd, I2C_RDWR, &xfer) < 0)
    {
        return (errno != 0) ? -errno : -EIO;
    }

    return 0;
}

int ocp_recov_open(struct ocp_recov_ctx* ctx, int bus, uint16_t addr)
{
    char path[32];
    int fd;

    if (ctx == NULL || bus < 0)
    {
        return -EINVAL;
    }

    snprintf(path, sizeof(path), "/dev/i2c-%d", bus);

    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        return (errno != 0) ? -errno : -EIO;
    }

    *ctx = (struct ocp_recov_ctx){
        .fd = fd,
        .addr = addr,
        .chunk_size = OCP_RECOV_CHUNK_SIZE_MAX,
        .cms = OCP_RECOV_CMS_DEFAULT,
        .transfer = ocp_i2c_transfer,
        .transfer_user = ctx,
    };

    return 0;
}

int ocp_recov_init_transport(struct ocp_recov_ctx* ctx,
                             ocp_recov_transfer_fn fn, void* user,
                             uint16_t addr)
{
    if (ctx == NULL || fn == NULL)
    {
        return -EINVAL;
    }

    *ctx = (struct ocp_recov_ctx){
        .fd = -1,
        .addr = addr,
        .chunk_size = OCP_RECOV_CHUNK_SIZE_MAX,
        .cms = OCP_RECOV_CMS_DEFAULT,
        .transfer = fn,
        .transfer_user = user,
    };

    return 0;
}

void ocp_recov_close(struct ocp_recov_ctx* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->fd >= 0)
    {
        close(ctx->fd);
        ctx->fd = -1;
    }

    ctx->transfer = NULL;
    ctx->transfer_user = NULL;
}
