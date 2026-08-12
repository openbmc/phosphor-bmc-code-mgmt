// SPDX-License-Identifier: Apache-2.0

#include "ocp/ocp_recovery.hpp"

#include <algorithm>
#include <array>
#include <thread>
#include <utility>

namespace ocp::recovery
{

/* Response payload lengths (framing is stripped by the Transport) */
constexpr size_t respLenProtCap = 15;
constexpr size_t respLenDeviceStatus = 24;
constexpr size_t respLenRecoveryStatus = 2;
constexpr size_t respLenIndirectStatus = 6;

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
    if (payload.size() > chunkSizeMax)
    {
        return failure(std::errc::invalid_argument);
    }

    if (auto ec = transport.write(cmd, payload))
    {
        return std::unexpected(ec);
    }
    return {};
}

std::expected<size_t, std::error_code> Target::commandRead(
    Cmd cmd, std::span<uint8_t> resp, size_t minLen)
{
    if (resp.empty() || resp.size() > responseSizeMax)
    {
        return failure(std::errc::invalid_argument);
    }

    const auto len = transport.read(cmd, resp);
    if (!len)
    {
        return std::unexpected(len.error());
    }

    if (*len < minLen || *len > resp.size())
    {
        return failure(std::errc::protocol_error);
    }

    return len;
}

std::expected<ProtCap, std::error_code> Target::getProtCap()
{
    std::array<uint8_t, respLenProtCap> resp{};

    const auto len = commandRead(Cmd::protCap, resp);
    if (!len)
    {
        return std::unexpected(len.error());
    }

    const auto magic = std::span(resp).first(protCapMagic.size());
    if (*len < respLenProtCap || !std::ranges::equal(magic, protCapMagic))
    {
        return failure(std::errc::protocol_error);
    }

    return ProtCap{
        .magic = std::string(magic.begin(), magic.end()),
        .major = resp[8],
        .minor = resp[9],
        .caps = getLe16(std::span(resp).subspan(10)),
        .numCms = resp[12],
        .maxRespTime = resp[13],
        .heartbeatPeriod = resp[14],
    };
}

std::expected<std::vector<uint8_t>, std::error_code> Target::getDeviceId(
    size_t maxLen)
{
    std::array<uint8_t, responseSizeMax> resp{};

    if (maxLen == 0 || maxLen > responseSizeMax)
    {
        return failure(std::errc::invalid_argument);
    }

    const auto len = commandRead(Cmd::deviceId, std::span(resp).first(maxLen));
    if (!len)
    {
        return std::unexpected(len.error());
    }

    /* Pass the descriptor through exactly as reported, up to maxLen;
     * vendor-specific IDs may use any length up to responseSizeMax. */
    const auto id = std::span(resp).first(*len);
    return std::vector<uint8_t>(id.begin(), id.end());
}

std::expected<DeviceStatusInfo, std::error_code> Target::getDeviceStatus()
{
    std::array<uint8_t, respLenDeviceStatus> resp{};

    const auto len = commandRead(Cmd::deviceStatus, resp);
    if (!len)
    {
        return std::unexpected(len.error());
    }

    if (*len < 4)
    {
        return failure(std::errc::protocol_error);
    }

    return DeviceStatusInfo{
        .status = DeviceStatus{resp[0]},
        .protocolError = ProtocolError{resp[1]},
        .reason = ReasonCode{getLe16(std::span(resp).subspan(2))},
    };
}

std::expected<RecoveryStatusInfo, std::error_code> Target::getRecoveryStatus()
{
    std::array<uint8_t, respLenRecoveryStatus> resp{};

    const auto len = commandRead(Cmd::recoveryStatus, resp);
    if (!len)
    {
        return std::unexpected(len.error());
    }

    return RecoveryStatusInfo{
        .status = RecoveryStatus{static_cast<uint8_t>(resp[0] & 0x0F)},
        .imageIndex = static_cast<uint8_t>((resp[0] >> 4) & 0x0F),
        .vendorStatus = static_cast<uint8_t>((*len >= 2) ? resp[1] : 0),
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

    const auto len = commandRead(Cmd::indirectStatus, resp);
    if (!len)
    {
        return std::unexpected(len.error());
    }

    /* Some devices answer acknowledge polls with a short, status-only
     * payload; tolerate that and report the size only when present. */
    return IndirectStatusInfo{
        .status = resp[0],
        .ack = (resp[0] & indirectStatusAckMask) != 0,
        .sizeUnits = (*len >= respLenIndirectStatus)
                         ? getLe32(std::span(resp).subspan(2))
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
    std::array<uint8_t, responseSizeMax> resp{};

    if (buf.empty())
    {
        return failure(std::errc::invalid_argument);
    }

    const size_t want = std::min(buf.size(), responseSizeMax);

    /* A zero length is valid here: it means the window is drained. */
    const auto len =
        commandRead(Cmd::indirectData, std::span(resp).first(want), 0);
    if (!len)
    {
        return std::unexpected(len.error());
    }

    std::ranges::copy(std::span(resp).first(*len), buf.begin());

    return *len;
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
