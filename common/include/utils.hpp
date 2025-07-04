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

template <typename>
inline constexpr bool always_false = false;

/**
 * @brief Constructs a vector of bytes (`std::vector<uint8_t>`) from a variable
 *        number of arguments, which can include enums, integral values,
 *        and initializer lists.
 *
 * This function is useful when building byte packets or command sequences
 * to be sent over communication protocols (e.g., I2C, UART, SPI).
 *
 * @tparam Args Types of arguments to convert into bytes
 * @param args The values to encode into the byte vector
 * @return std::vector<uint8_t> A flattened list of bytes
 *
 * @note Passing unsupported types will trigger a compile-time static_assert.
 * @note Endianness: Multi-byte integers use little-endian order.
 *
 * @code
 * enum class Command : uint8_t { Start = 0x01 };
 * auto buf = buildByteVector(Command::Start, 0x1234, {0xAA, 0xBB});
 * // Result: { 0x01, 0x34, 0x12, 0xAA, 0xBB }
 * @endcode
 */
template <typename... Args>
std::vector<uint8_t> buildByteVector(Args&&... args)
{
    std::vector<uint8_t> buf;

    auto append = [&](auto&& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_enum_v<T>)
        {
            buf.push_back(static_cast<uint8_t>(value));
        }
        else if constexpr (std::is_integral_v<T>)
        {
            for (size_t i = 0; i < sizeof(T); ++i)
            {
                buf.push_back(static_cast<uint8_t>(value >> (i * 8)));
            }
        }
        else if constexpr (std::is_same_v<T, std::initializer_list<uint8_t>>)
        {
            buf.insert(buf.end(), value.begin(), value.end());
        }
        else
        {
            static_assert(always_false<T>,
                          "Unsupported type in buildByteVector");
        }
    };

    (append(std::forward<Args>(args)), ...);

    return buf;
}
