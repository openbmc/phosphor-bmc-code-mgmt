#pragma once

#include "common/include/i2c/i2c.hpp"
#include "common/include/pmbus.hpp"
#include "i2c-vr/vr.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <type_traits>
#include <vector>

namespace phosphor::software::VR
{

enum class ATE : uint8_t
{
    ConfigId = 0,
    PageNum,
    RegAddrHex,
    RegAddrDec,
    RegName,
    RegDataHex,
    RegDataDec,
    WriteType,
    ColCount,
};

enum class MPSPage : uint8_t
{
    PAGE0 = 0,
    PAGE1,
    PAGE2,
    PAGE3,
    PAGE4,
};

template <typename>
inline constexpr bool always_false = false;

class MPSVoltageRegulator : public VoltageRegulator
{
  public:
    MPSVoltageRegulator(sdbusplus::async::context& ctx, uint16_t bus,
                        uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
    {}

  protected:
    template <
        typename... Args,
        typename = std::enable_if_t<
            !(sizeof...(Args) == 1 &&
              std::is_same_v<
                  std::decay_t<std::tuple_element_t<0, std::tuple<Args...>>>,
                  std::vector<uint8_t>>)>>
    sdbusplus::async::task<bool> writeData(Args&&... args)
    {
        std::vector<uint8_t> buf;
        buf.reserve(sizeof...(Args));

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
            else if constexpr (std::is_same_v<T,
                                              std::initializer_list<uint8_t>>)
            {
                buf.insert(buf.end(), std::begin(value), std::end(value));
            }
            else
            {
                static_assert(always_false<T>,
                              "Unsupported argument to writeData()");
            }
        };

        (append(std::forward<Args>(args)), ...);

        co_return co_await writeData(buf);
    }
    sdbusplus::async::task<bool> writeData(const std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> setPage(MPSPage page);

    phosphor::i2c::I2C i2cInterface;
};

} // namespace phosphor::software::VR
