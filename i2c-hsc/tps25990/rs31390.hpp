#pragma once

#include "common/include/pmbus.hpp"
#include "tps25990.hpp"

namespace phosphor::software::HSC
{

class RS31390 : public TPS25990
{
  public:
    RS31390(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
            TPS25990(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> getCheckSum(uint32_t* sum) final;
    sdbusplus::async::task<bool> parseImage(const uint8_t* image,
                                            size_t imageSize) final;
};

} // namespace phosphor::software::HSC
