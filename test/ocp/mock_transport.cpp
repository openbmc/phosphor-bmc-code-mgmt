#include "mock_transport.hpp"

#include <algorithm>
#include <utility>

std::vector<uint8_t> MockTransport::commandSequence() const
{
    std::vector<uint8_t> seq;
    seq.reserve(writes.size());
    for (const auto& w : writes)
    {
        seq.push_back(w[0]);
    }
    return seq;
}

std::error_code MockTransport::checkErrors(uint8_t cmd) const
{
    if (transferError)
    {
        return transferError;
    }

    if (auto errIt = commandErrors.find(cmd); errIt != commandErrors.end())
    {
        return errIt->second;
    }

    return {};
}

std::vector<uint8_t> MockTransport::nextResponse(uint8_t cmd)
{
    if (auto queueIt = queuedResponses.find(cmd);
        queueIt != queuedResponses.end() && !queueIt->second.empty())
    {
        auto resp = std::move(queueIt->second.front());
        queueIt->second.pop_front();
        return resp;
    }

    if (auto respIt = staticResponses.find(cmd);
        respIt != staticResponses.end())
    {
        return respIt->second;
    }

    /* No scripted response: an all-zero wire read (count 0). */
    return {0};
}

std::error_code MockTransport::write(ocp::recovery::Cmd cmd,
                                     std::span<const uint8_t> payload)
{
    const uint8_t code = std::to_underlying(cmd);

    if (auto ec = checkErrors(code))
    {
        return ec;
    }

    std::vector<uint8_t> framed{code, static_cast<uint8_t>(payload.size())};
    framed.insert(framed.end(), payload.begin(), payload.end());
    writes.push_back(std::move(framed));

    return {};
}

std::expected<size_t, std::error_code> MockTransport::read(
    ocp::recovery::Cmd cmd, std::span<uint8_t> resp)
{
    const uint8_t code = std::to_underlying(cmd);

    if (auto ec = checkErrors(code))
    {
        return std::unexpected(ec);
    }

    /* The command byte is written before the repeated-start read; the
     * wire read covers the byte count plus the caller's window. */
    writes.push_back({code});
    reads.push_back(1 + resp.size());

    const auto wire = nextResponse(code);

    const size_t len = wire.empty() ? 0 : wire[0];
    if (len > resp.size())
    {
        return std::unexpected(std::make_error_code(std::errc::protocol_error));
    }

    std::ranges::fill(resp, uint8_t{0});
    const size_t available = std::min(len, wire.size() - 1);
    std::ranges::copy(std::span(wire).subspan(1, available), resp.begin());

    return len;
}
