#pragma once

#include <sdbusplus/async.hpp>

/**
 * @brief Asynchronously executes a shell command.
 * @param ctx Async context for monitoring the pipe.
 * @param cmd Shell command to execute.
 * @return Task resolving to child exit status or -1 on failure.
 */
sdbusplus::async::task<int> asyncSystem(sdbusplus::async::context& ctx,
                                        const std::string& cmd);
