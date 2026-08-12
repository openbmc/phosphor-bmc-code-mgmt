#include "mock_transport.hpp"

#include <algorithm>

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

std::error_code MockTransport::transfer(std::span<const uint8_t> wbuf,
                                        std::span<uint8_t> rbuf)
{
    const uint8_t cmd = wbuf[0];

    if (transferError)
    {
        return transferError;
    }

    if (auto errIt = commandErrors.find(cmd); errIt != commandErrors.end())
    {
        return errIt->second;
    }

    writes.emplace_back(wbuf.begin(), wbuf.end());

    if (rbuf.empty())
    {
        return {};
    }

    reads.push_back(rbuf.size());

    std::ranges::fill(rbuf, uint8_t{0});

    if (auto queueIt = queuedResponses.find(cmd);
        queueIt != queuedResponses.end() && !queueIt->second.empty())
    {
        const auto& resp = queueIt->second.front();
        std::ranges::copy(
            std::span(resp).first(std::min(rbuf.size(), resp.size())),
            rbuf.begin());
        queueIt->second.pop_front();
        return {};
    }

    if (auto respIt = staticResponses.find(cmd);
        respIt != staticResponses.end())
    {
        const auto& resp = respIt->second;
        std::ranges::copy(
            std::span(resp).first(std::min(rbuf.size(), resp.size())),
            rbuf.begin());
    }

    return {};
}
