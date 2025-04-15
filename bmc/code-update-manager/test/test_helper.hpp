#pragma once

#include <future>
#include <type_traits>
#include <tuple>
#include <functional>
#include <sdbusplus/message/native_types.hpp>

// Test helper for executing async tasks in tests
namespace test
{
    /**
     * @brief Execute a task and wait for its completion in a test context
     * 
     * This provides a simplified way to execute async tasks in a test context
     * without requiring the full sdbusplus::async::sync_wait implementation.
     * 
     * @tparam T The return type of the task
     * @param task The task to execute
     * @return The result of the task 
     */
    template <typename F>
    auto execute_task(F&& task)
    {
        // Use a promise to get the result
        std::promise<std::tuple<sdbusplus::message::details::string_path_wrapper>> promise;

        task([&promise](const auto& result) { promise.set_value(result); });

        return promise.get_future().get();
    }
    
    // Specialized for void return
    template <typename F>
    void execute_task_void(F&& task)
    {
        task([](){});
    }
} 