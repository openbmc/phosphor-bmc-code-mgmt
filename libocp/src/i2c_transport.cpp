// SPDX-License-Identifier: Apache-2.0

#include "ocp/ocp_recovery.hpp"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <format>
#include <utility>

namespace ocp::recovery
{

static std::error_code errnoCode(int fallback)
{
    return std::error_code((errno != 0) ? errno : fallback,
                           std::generic_category());
}

std::expected<I2CTransport, std::error_code> I2CTransport::open(uint16_t bus,
                                                                uint16_t addr)
{
    const std::string path = std::format("/dev/i2c-{}", bus);

    const int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        return std::unexpected(errnoCode(EIO));
    }

    return I2CTransport(fd, addr);
}

I2CTransport::~I2CTransport()
{
    if (fd >= 0)
    {
        ::close(fd);
    }
}

I2CTransport::I2CTransport(I2CTransport&& other) noexcept :
    fd(std::exchange(other.fd, -1)), addr(other.addr)
{}

I2CTransport& I2CTransport::operator=(I2CTransport&& other) noexcept
{
    if (this != &other)
    {
        if (fd >= 0)
        {
            ::close(fd);
        }
        fd = std::exchange(other.fd, -1);
        addr = other.addr;
    }
    return *this;
}

std::error_code I2CTransport::write(Cmd cmd, std::span<const uint8_t> payload)
{
    /* SMBus block write: [command, byte count, payload...] */
    std::array<uint8_t, 2 + chunkSizeMax> wbuf{};

    if (payload.size() > chunkSizeMax)
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    wbuf[0] = std::to_underlying(cmd);
    wbuf[1] = static_cast<uint8_t>(payload.size());
    std::ranges::copy(payload, wbuf.begin() + 2);

    return transfer(std::span(wbuf).first(2 + payload.size()), {});
}

std::expected<size_t, std::error_code> I2CTransport::read(
    Cmd cmd, std::span<uint8_t> resp)
{
    /* SMBus block read: [command] write, then a repeated-start read of
     * [byte count, payload...] */
    const std::array<uint8_t, 1> wbuf = {std::to_underlying(cmd)};
    std::array<uint8_t, 1 + responseSizeMax> rbuf{};

    if (resp.empty() || resp.size() > responseSizeMax)
    {
        return std::unexpected(
            std::make_error_code(std::errc::invalid_argument));
    }

    if (auto ec = transfer(wbuf, std::span(rbuf).first(1 + resp.size())))
    {
        return std::unexpected(ec);
    }

    /* A byte count beyond the clocked window means the device produced
     * more than the caller asked for; the payload is not usable. */
    const size_t len = rbuf[0];
    if (len > resp.size())
    {
        return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }

    std::ranges::copy(std::span(rbuf).subspan(1, len), resp.begin());
    return len;
}

std::error_code I2CTransport::transfer(std::span<const uint8_t> wbuf,
                                       std::span<uint8_t> rbuf)
{
    /* Largest write message: [command, byte count, payload...] */
    std::array<uint8_t, 2 + chunkSizeMax> wcopy{};

    if (fd < 0 || wbuf.empty() || wbuf.size() > wcopy.size() ||
        rbuf.size() > UINT16_MAX)
    {
        return std::make_error_code(std::errc::invalid_argument);
    }

    /* The kernel message buffer is not const-qualified; stage the write
     * in a local copy rather than casting constness away. */
    std::ranges::copy(wbuf, wcopy.begin());

    std::array<i2c_msg, 2> msgs{};

    msgs[0].addr = addr;
    msgs[0].flags = 0;
    msgs[0].len = static_cast<uint16_t>(wbuf.size());
    msgs[0].buf = wcopy.data();

    if (!rbuf.empty())
    {
        msgs[1].addr = addr;
        msgs[1].flags = I2C_M_RD;
        msgs[1].len = static_cast<uint16_t>(rbuf.size());
        msgs[1].buf = rbuf.data();
    }

    i2c_rdwr_ioctl_data xfer{};
    xfer.msgs = msgs.data();
    xfer.nmsgs = rbuf.empty() ? 1 : 2;

    if (ioctl(fd, I2C_RDWR, &xfer) < 0)
    {
        return errnoCode(EIO);
    }

    return {};
}

} // namespace ocp::recovery
