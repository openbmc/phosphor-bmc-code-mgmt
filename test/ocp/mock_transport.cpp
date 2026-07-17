#include "mock_transport.hpp"

#include <algorithm>
#include <cstring>

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

int mockTransfer(void* user, const uint8_t* wbuf, size_t wlen, uint8_t* rbuf,
                 size_t rlen)
{
    auto* mock = static_cast<MockTransport*>(user);
    const uint8_t cmd = wbuf[0];

    if (mock->transferError != 0)
    {
        return mock->transferError;
    }

    auto errIt = mock->commandErrors.find(cmd);
    if (errIt != mock->commandErrors.end())
    {
        return errIt->second;
    }

    mock->writes.emplace_back(wbuf, wbuf + wlen);

    if (rlen == 0)
    {
        return 0;
    }

    std::memset(rbuf, 0, rlen);

    auto queueIt = mock->queuedResponses.find(cmd);
    if (queueIt != mock->queuedResponses.end() && !queueIt->second.empty())
    {
        const auto& resp = queueIt->second.front();
        std::memcpy(rbuf, resp.data(), std::min(rlen, resp.size()));
        queueIt->second.pop_front();
        return 0;
    }

    auto respIt = mock->staticResponses.find(cmd);
    if (respIt != mock->staticResponses.end())
    {
        const auto& resp = respIt->second;
        std::memcpy(rbuf, resp.data(), std::min(rlen, resp.size()));
        return 0;
    }

    return 0;
}
