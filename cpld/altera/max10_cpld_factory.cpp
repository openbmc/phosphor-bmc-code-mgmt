#include "max10_cpld_factory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdlib>
#include <string>

namespace phosphor::software::cpld
{

std::unique_ptr<Max10StandardCPLD> Max10CPLDFactory::getMax10CPLD() const
{
    const auto chipModelName = getMax10ChipName(chipEnum, max10DataType::model);
    const auto configTypeName = getMax10ChipName(chipEnum, max10DataType::type);

    if (chipModelName.empty() || configTypeName.empty())
    {
        lg2::error("Unsupported MAX10 CPLD chip enum");
        return nullptr;
    }

+    return std::make_unique<Max10StandardCPLD>(ctx, bus, address, chipModelName,
+                                               configTypeName, Max10Profile{});
}

sdbusplus::async::task<bool> Max10CPLDFactory::updateFirmware(
    bool /*force*/, const uint8_t* image, size_t imageSize,
    std::function<bool(int)> progressCallBack)
{
    lg2::info("Updating MAX10 CPLD firmware");
    auto cpldManager = getMax10CPLD();
    if (cpldManager == nullptr)
    {
        lg2::error("MAX10 CPLD manager is not initialized.");
        co_return false;
    }

    co_return co_await cpldManager->updateFirmware(false, image, imageSize,
                                                   std::move(progressCallBack));
}

sdbusplus::async::task<bool> Max10CPLDFactory::getVersion(std::string& version)
{
    lg2::info("Getting MAX10 CPLD version");
    auto cpldManager = getMax10CPLD();
    if (cpldManager == nullptr)
    {
        lg2::error("MAX10 CPLD manager is not initialized.");
        co_return false;
    }

    co_return co_await cpldManager->getVersion(version);
}

namespace
{
using namespace phosphor::software::cpld;

const bool vendorRegistered = [] {
    for (const auto& [chipEnum, info] : getSupportedDeviceMap())
    {
        (void)info;
        const auto typeName = getMax10ChipName(chipEnum, max10DataType::type);
        if (typeName.empty())
        {
            continue;
        }

        CPLDFactory::instance().registerCPLD(
            typeName, [chipEnum](sdbusplus::async::context& ctx,
                                 const std::string& chipName, uint16_t bus,
                                 uint8_t address) {
                return std::make_unique<Max10CPLDFactory>(
                    ctx, chipName, chipEnum, bus, address);
            });
    }

    return true;
}();

} // namespace
} // namespace phosphor::software::cpld
