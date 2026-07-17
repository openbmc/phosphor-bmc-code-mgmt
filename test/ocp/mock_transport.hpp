#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

/* Register-level scripted transport: records every write message and
 * answers reads per command code, either from a queue (consumed in order)
 * or from a static response. */
struct MockTransport
{
    std::vector<std::vector<uint8_t>> writes;
    std::map<uint8_t, std::vector<uint8_t>> staticResponses;
    std::map<uint8_t, std::deque<std::vector<uint8_t>>> queuedResponses;
    std::map<uint8_t, int> commandErrors;
    int transferError = 0;

    std::vector<uint8_t> commandSequence() const;
};

/* ocp_recov_transfer_fn-compatible transport; pass a MockTransport* as
 * 'user'. */
int mockTransfer(void* user, const uint8_t* wbuf, size_t wlen, uint8_t* rbuf,
                 size_t rlen);
