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

/**
 * @brief Convert bytes to an integer of the given type.
 *
 * @tparam IntegerType Output integer type (e.g., uint16_t, uint32_t).
 * @tparam Container A container of uint8_t bytes.
 * @param data Byte data to convert.
 * @param bigEndian Set true for big-endian order; false for little-endian.
 * @return Converted integer.
 */
template <typename IntegerType, typename Container>
IntegerType bytesToInt(const Container& data, bool bigEndian = false)
{
    static_assert(std::is_integral_v<IntegerType>,
                  "IntegerType must be an integral type");
    static_assert(std::is_same_v<typename Container::value_type, uint8_t>,
                  "Container must hold uint8_t elements");

    constexpr size_t maxBytes = sizeof(IntegerType);
    size_t size = std::min(data.size(), maxBytes);

    IntegerType result = 0;
    for (size_t i = 0; i < size; ++i)
    {
        size_t shift = bigEndian ? (size - 1 - i) * 8 : i * 8;
        result |= static_cast<IntegerType>(data[i]) << shift;
    }

    return result;
}
