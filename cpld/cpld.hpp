#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

#include <functional>
#include <map>
#include <string>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::manager;

namespace phosphor::software::cpld::device
{

class CPLD : public Device
{
  public:
    CPLD(sdbusplus::async::context& ctx, const std::string& vendor,
         const std::string& chipname, const uint16_t& bus,
         const uint8_t& address, SoftwareConfig& config,
         SoftwareManager* parent) :
        Device(ctx, config, parent, {RequestedApplyTimes::Immediate}),
        vendor(vendor), chipname(chipname), bus(bus), address(address)
    {
        auto it = vendorMap.find(vendor);
        if (it != vendorMap.end())
        {
            selectedVendorUpdateFunction = it->second;
        }
        else
        {
            error("Vendor {VENDOR} not supported.", "VENDOR", vendor);
            selectedVendorUpdateFunction = nullptr;
        }
    }

    using Device::softwareCurrent;
    // NOLINTBEGIN(readability-static-accessed-through-instance)
    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final
    // NOLINTBEGIN(readability-static-accessed-through-instance)
    {
        if (selectedVendorUpdateFunction)
        {
            debug(
                "Called device specific update function with image size {SIZE}",
                "SIZE", image_size);

            deviceSpecificUpdateFunctionCalled = true;

            co_return selectedVendorUpdateFunction(image, image_size);
        }
        else
        {
            co_return false;
        }
    }

  private:
    std::string vendor;
    std::string chipname;
    uint16_t bus;
    uint8_t address;
    bool deviceSpecificUpdateFunctionCalled = false;

    bool latticeUpdate(const uint8_t* image, size_t image_size);

    // init vendorMap
    std::map<std::string, std::function<bool(const uint8_t*, size_t)>>
        vendorMap = {
            {"lattice", [this](const uint8_t* image, size_t image_size) {
                 lg2::info("lattice device found.");
                 return latticeUpdate(image, image_size);
             }}};

    // current selected vendor update function
    std::function<bool(const uint8_t*, size_t)> selectedVendorUpdateFunction =
        nullptr;
};

} // namespace phosphor::software::cpld::device
