#pragma once

#include <ocp/ocp_recovery.hpp>

#include <cstdint>
#include <deque>
#include <expected>
#include <map>
#include <span>
#include <system_error>
#include <vector>

/* Scripted transport emulating an SMBus block read/write device: frames
 * every write as [command, byte count, payload...] and records it, and
 * answers reads per command code from SMBus-framed ([byte count,
 * payload...]) responses, either from a queue (consumed in order) or
 * from a static response. */
struct MockTransport : ocp::recovery::Transport
{
    std::vector<std::vector<uint8_t>> writes; /* SMBus-framed messages */
    std::vector<size_t> reads; /* wire read lengths (count + window) */
    std::map<uint8_t, std::vector<uint8_t>> staticResponses;
    std::map<uint8_t, std::deque<std::vector<uint8_t>>> queuedResponses;
    std::map<uint8_t, std::error_code> commandErrors;
    std::error_code transferError;

    [[nodiscard]] std::vector<uint8_t> commandSequence() const;

    std::error_code write(ocp::recovery::Cmd cmd,
                          std::span<const uint8_t> payload) override;

    std::expected<size_t, std::error_code> read(
        ocp::recovery::Cmd cmd, std::span<uint8_t> resp) override;

  private:
    [[nodiscard]] std::error_code checkErrors(uint8_t cmd) const;
    [[nodiscard]] std::vector<uint8_t> nextResponse(uint8_t cmd);
};
