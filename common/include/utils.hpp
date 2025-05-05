#pragma once

#include <sdbusplus/async.hpp>

/**
 * @brief Executes a shell command asynchronously and returns its exit code.
 * @param ctx The asynchronous sdbusplus context used to monitor the pipe.
 * @param cmd The shell command to execute.
 * @return A task that resolves to the exit code of the command, or -1 on error.
 */
sdbusplus::async::task<int> asyncSystem(sdbusplus::async::context& ctx,
                                        const std::string& cmd);
