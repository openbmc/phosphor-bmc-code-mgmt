#pragma once

#include <sdbusplus/async.hpp>

/**
 * @brief Asynchronously executes a shell command.
 * @param ctx Async context for monitoring the pipe.
 * @param cmd Shell command to execute.
 * @return Task resolving to true on success (exit code 0), false otherwise.
 */
sdbusplus::async::task<bool> asyncSystem(sdbusplus::async::context& ctx,
                                         const std::string& cmd);
