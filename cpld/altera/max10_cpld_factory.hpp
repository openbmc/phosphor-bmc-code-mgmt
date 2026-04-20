#pragma once
#include "cpld/cpld_interface.hpp"
#include "cpld/altera/max10_cpld.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <sdbusplus/async.hpp>

namespace phosphor::software::cpld
{

class Max10CPLDFactory : public CPLDInterface
{
  public:
    Max10CPLDFactory(sdbusplus::async::context& ctx, const std::string& chipName,
                     max10Chip chipEnum, uint16_t bus, uint8_t address) :
        CPLDInterface(ctx, chipName, bus, address),
        chipEnum(chipEnum)
    {}

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progressCallBack) final;

    sdbusplus::async::task<bool> getVersion(std::string& version) final;

  private:
    std::unique_ptr<Max10CPLD> getMax10CPLD() const;

    max10Chip chipEnum;
};

} // namespace phosphor::software::cpld
