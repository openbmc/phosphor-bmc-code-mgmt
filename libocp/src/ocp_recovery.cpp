// SPDX-License-Identifier: Apache-2.0

#include "ocp/ocp_recovery.hpp"

#include <algorithm>
#include <array>
#include <thread>
#include <utility>

namespace ocp::recovery
{

/* SMBus block responses lead with the byte count */
constexpr size_t respLenProtCap = 16;
constexpr size_t respLenDeviceStatus = 25;
constexpr size_t respLenRecoveryStatus = 3;
constexpr size_t respLenIndirectStatus = 7;

/* One full SMBus block: byte count + up to 255 payload bytes */
constexpr size_t respLenBlockMax = 1 + 255;

constexpr uint8_t indirectStatusAckMask = 0x04;

constexpr std::array<uint8_t, 8> protCapMagic = {'O', 'C', 'P', ' ',
                                                 'R', 'E', 'C', 'V'};

namespace
{

std::unexpected<std::error_code> failure(std::errc code)
{
    return std::unexpected(std::make_error_code(code));
}

uint16_t getLe16(std::span<const uint8_t> b)
{
    return static_cast<uint16_t>(b[0] | (uint16_t{b[1]} << 8));
}

uint32_t getLe32(std::span<const uint8_t> b)
{
    return b[0] | (uint32_t{b[1]} << 8) | (uint32_t{b[2]} << 16) |
           (uint32_t{b[3]} << 24);
}

} // namespace

std::expected<void, std::error_code> Target::command(
    Cmd cmd, std::span<const uint8_t> payload)
{
    std::array<uint8_t, 2 + chunkSizeMax> buf{};

    if (payload.size() > chunkSizeMax)
    {
        return failure(std::errc::invalid_argument);
    }

    buf[0] = std::to_underlying(cmd);
    buf[1] = static_cast<uint8_t>(payload.size());
    std::ranges::copy(payload, buf.begin() + 2);

    if (auto ec =
            transport.transfer(std::span(buf).first(2 + payload.size()), {}))
    {
        return std::unexpected(ec);
    }
    return {};
}

std::expected<size_t, std::error_code> Target::commandRead(
    Cmd cmd, std::span<uint8_t> resp, size_t minCount)
{
    const std::array<uint8_t, 1> wbuf = {std::to_underlying(cmd)};

    if (resp.size() < 2)
    {
        return failure(std::errc::invalid_argument);
    }

    if (auto ec = transport.transfer(wbuf, resp))
    {
        return std::unexpected(ec);
    }

    /* resp[0] is the SMBus block byte count of the payload that follows */
    const size_t count = resp[0];
    if (count < minCount || count > resp.size() - 1)
    {
        return failure(std::errc::protocol_error);
    }

    return count;
}

std::expected<ProtCap, std::error_code> Target::getProtCap()
{
    std::array<uint8_t, respLenProtCap> resp{};

    const auto count = commandRead(Cmd::protCap, resp);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    const auto magic = std::span(resp).subspan(1, protCapMagic.size());
    if (*count < respLenProtCap - 1 || !std::ranges::equal(magic, protCapMagic))
    {
        return failure(std::errc::protocol_error);
    }

    return ProtCap{
        .magic = std::string(magic.begin(), magic.end()),
        .major = resp[9],
        .minor = resp[10],
        .caps = getLe16(std::span(resp).subspan(11)),
        .numCms = resp[13],
        .maxRespTime = resp[14],
        .heartbeatPeriod = resp[15],
    };
}

std::expected<std::vector<uint8_t>, std::error_code> Target::getDeviceId(
    size_t maxLen)
{
    std::array<uint8_t, respLenBlockMax> resp{};

    if (maxLen == 0 || maxLen > respLenBlockMax - 1)
    {
        return failure(std::errc::invalid_argument);
    }

    const auto count =
        commandRead(Cmd::deviceId, std::span(resp).first(maxLen + 1));
    if (!count)
    {
        return std::unexpected(count.error());
    }

    /* Pass the descriptor through exactly as reported, up to maxLen;
     * vendor-specific IDs may use any length up to a full SMBus block. */
    const auto id = std::span(resp).subspan(1, std::min(*count, maxLen));
    return std::vector<uint8_t>(id.begin(), id.end());
}

std::expected<DeviceStatusInfo, std::error_code> Target::getDeviceStatus()
{
    std::array<uint8_t, respLenDeviceStatus> resp{};

    const auto count = commandRead(Cmd::deviceStatus, resp);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    if (*count < 4)
    {
        return failure(std::errc::protocol_error);
    }

    return DeviceStatusInfo{
        .status = DeviceStatus{resp[1]},
        .protocolError = ProtocolError{resp[2]},
        .reason = ReasonCode{getLe16(std::span(resp).subspan(3))},
    };
}

std::expected<RecoveryStatusInfo, std::error_code> Target::getRecoveryStatus()
{
    std::array<uint8_t, respLenRecoveryStatus> resp{};

    const auto count = commandRead(Cmd::recoveryStatus, resp);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    return RecoveryStatusInfo{
        .status = RecoveryStatus{static_cast<uint8_t>(resp[1] & 0x0F)},
        .imageIndex = static_cast<uint8_t>((resp[1] >> 4) & 0x0F),
        .vendorStatus = static_cast<uint8_t>((*count >= 2) ? resp[2] : 0),
    };
}

std::expected<void, std::error_code> Target::forceRecovery()
{
    /* device reset, enter recovery on reset, enable interface mastering */
    constexpr std::array<uint8_t, 3> payload = {0x01, 0x0F, 0x01};

    return command(Cmd::deviceReset, payload);
}

std::expected<void, std::error_code> Target::recoveryCtrl(
    uint8_t window, ImageSelection imageSel, Activation activation)
{
    const std::array<uint8_t, 3> payload = {
        window, std::to_underlying(imageSel), std::to_underlying(activation)};

    return command(Cmd::recoveryCtrl, payload);
}

std::expected<void, std::error_code> Target::indirectCtrl(uint8_t window,
                                                          uint32_t offset)
{
    const std::array<uint8_t, 6> payload = {
        window,
        0x00, /* reserved */
        static_cast<uint8_t>(offset & 0xFF),
        static_cast<uint8_t>((offset >> 8) & 0xFF),
        static_cast<uint8_t>((offset >> 16) & 0xFF),
        static_cast<uint8_t>((offset >> 24) & 0xFF),
    };

    return command(Cmd::indirectCtrl, payload);
}

std::expected<IndirectStatusInfo, std::error_code> Target::indirectStatus()
{
    std::array<uint8_t, respLenIndirectStatus> resp{};

    const auto count = commandRead(Cmd::indirectStatus, resp);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    /* Some devices answer acknowledge polls with a short, status-only
     * payload; tolerate that and report the size only when present. */
    return IndirectStatusInfo{
        .status = resp[1],
        .ack = (resp[1] & indirectStatusAckMask) != 0,
        .sizeUnits = (*count >= respLenIndirectStatus - 1)
                         ? getLe32(std::span(resp).subspan(3))
                         : 0,
    };
}

std::expected<void, std::error_code> Target::writeIndirectData(
    std::span<const uint8_t> data)
{
    if (data.empty() || data.size() > chunkSize || chunkSize > chunkSizeMax)
    {
        return failure(std::errc::invalid_argument);
    }

    return command(Cmd::indirectData, data);
}

std::expected<size_t, std::error_code> Target::readIndirectData(
    std::span<uint8_t> buf)
{
    std::array<uint8_t, respLenBlockMax> resp{};

    if (buf.empty())
    {
        return failure(std::errc::invalid_argument);
    }

    const size_t want = std::min(buf.size(), respLenBlockMax - 1);

    /* A zero byte count is valid here: it means the window is drained. */
    const auto count =
        commandRead(Cmd::indirectData, std::span(resp).first(want + 1), 0);
    if (!count)
    {
        return std::unexpected(count.error());
    }

    const size_t len = std::min(*count, want);
    std::ranges::copy(std::span(resp).subspan(1, len), buf.begin());

    return len;
}

/* Blocking convenience layer */

std::expected<void, std::error_code> Target::waitWriteAck()
{
    for (unsigned int poll = 0; poll < ackPollRetries; poll++)
    {
        const auto status = indirectStatus();
        if (!status)
        {
            return std::unexpected(status.error());
        }
        if (status->ack)
        {
            return {};
        }
        std::this_thread::sleep_for(ackPollInterval);
    }

    return failure(std::errc::timed_out);
}

std::expected<void, std::error_code> Target::writeImage(
    std::span<const uint8_t> image, const ProgressFn& progress)
{
    if (image.empty() || chunkSize == 0 || chunkSize > chunkSizeMax)
    {
        return failure(std::errc::invalid_argument);
    }

    size_t offset = 0;
    while (offset < image.size())
    {
        const auto chunk =
            image.subspan(offset, std::min(chunkSize, image.size() - offset));

        std::expected<void, std::error_code> result =
            failure(std::errc::timed_out);

        for (unsigned int attempt = 0; attempt < chunkWriteRetries; attempt++)
        {
            if (auto written = writeIndirectData(chunk); !written)
            {
                return written;
            }

            result = waitWriteAck();
            if (result || result.error() != std::errc::timed_out)
            {
                break;
            }
        }
        if (!result)
        {
            return result;
        }

        offset += chunk.size();

        if (progress)
        {
            progress(offset, image.size());
        }
    }

    return {};
}

std::expected<void, std::error_code> Target::recover(
    std::span<const uint8_t> image, const ProgressFn& progress)
{
    if (image.empty())
    {
        return failure(std::errc::invalid_argument);
    }

    /* Tolerant: many devices do not implement PROT_CAP; only fail when
     * the device positively reports FIFO-only support. */
    if (const auto cap = getProtCap(); cap && cap->fifoOnly())
    {
        return failure(std::errc::not_supported);
    }

    auto status = getDeviceStatus();
    if (!status)
    {
        return std::unexpected(status.error());
    }

    if (status->status != DeviceStatus::recoveryMode)
    {
        if (auto forced = forceRecovery(); !forced)
        {
            return forced;
        }

        for (unsigned int poll = 0; poll < statusPollRetries; poll++)
        {
            std::this_thread::sleep_for(statusPollInterval);

            status = getDeviceStatus();
            if (status && status->status == DeviceStatus::recoveryMode)
            {
                break;
            }
        }
        if (!status || status->status != DeviceStatus::recoveryMode)
        {
            return failure(std::errc::timed_out);
        }
    }

    if (auto ctrl =
            recoveryCtrl(cms, ImageSelection::fromCms, Activation::none);
        !ctrl)
    {
        return ctrl;
    }

    if (auto ctrl = indirectCtrl(cms, 0); !ctrl)
    {
        return ctrl;
    }

    if (auto written = writeImage(image, progress); !written)
    {
        return written;
    }

    if (auto ctrl =
            recoveryCtrl(cms, ImageSelection::fromCms, Activation::activate);
        !ctrl)
    {
        return ctrl;
    }

    for (unsigned int poll = 0; poll < statusPollRetries; poll++)
    {
        if (const auto rec = getRecoveryStatus(); rec)
        {
            if (rec->status == RecoveryStatus::success)
            {
                return {};
            }
            if (rec->status >= RecoveryStatus::failed)
            {
                return failure(std::errc::io_error);
            }
        }
        std::this_thread::sleep_for(statusPollInterval);
    }

    return failure(std::errc::timed_out);
}

} // namespace ocp::recovery
