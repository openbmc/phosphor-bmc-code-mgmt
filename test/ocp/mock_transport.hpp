#pragma once

#include <ocp/ocp_recovery.hpp>

#include <cstdint>
#include <deque>
#include <map>
#include <span>
#include <system_error>
#include <vector>

/* Register-level scripted transport: records every write message and
 * answers reads per command code, either from a queue (consumed in order)
 * or from a static response. */
struct MockTransport : ocp::recovery::Transport
{
    std::vector<std::vector<uint8_t>> writes;
    std::vector<size_t> reads; /* requested read lengths, in order */
    std::map<uint8_t, std::vector<uint8_t>> staticResponses;
    std::map<uint8_t, std::deque<std::vector<uint8_t>>> queuedResponses;
    std::map<uint8_t, std::error_code> commandErrors;
    std::error_code transferError;

    [[nodiscard]] std::vector<uint8_t> commandSequence() const;

    std::error_code transfer(std::span<const uint8_t> wbuf,
                             std::span<uint8_t> rbuf) override;
};
