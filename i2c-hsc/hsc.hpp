#pragma once

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace phosphor::software::HSC
{

enum class HSCType
{
    TPS25990,
    RS31390
};

class HotSwapController
{
  public:
    explicit HotSwapController(sdbusplus::async::context& ctx) : ctx(ctx) {}
    virtual ~HotSwapController() = default;

    HotSwapController(HotSwapController& hsc) = delete;
    HotSwapController& operator=(HotSwapController other) = delete;
    HotSwapController(HotSwapController&& other) = delete;
    HotSwapController& operator=(HotSwapController&& other) = delete;

    // @brief Parses the firmware image into the configuration structure
    //        and verifies its correctness.
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                                     size_t imageSize) = 0;

    // @brief Applies update to the hot swap controller
    // @return sdbusplus::async::task<bool> true indicates success.
    virtual sdbusplus::async::task<bool> updateFirmware(bool force) = 0;

    virtual sdbusplus::async::task<bool> getCRC(uint32_t* checksum) = 0;

  protected:
    sdbusplus::async::context& ctx;
};

std::unique_ptr<HotSwapController> create(sdbusplus::async::context& ctx,
                                         enum HSCType hscType, uint16_t bus,
                                         uint16_t address);

bool stringToEnum(std::string& hscStr, HSCType& hscType);

} // namespace phosphor::software::HSC
