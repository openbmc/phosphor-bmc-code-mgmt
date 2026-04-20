#include "max10_cpld_factory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdlib>
#include <optional>
#include <string>

namespace phosphor::software::cpld
{
namespace
{
template <typename T>
std::optional<T> parseEnvUnsigned(const char* name)
{
    const char* s = std::getenv(name);
    if (!s || *s == '\0')
    {
        return std::nullopt;
    }

    try
    {
        unsigned long v = std::stoul(s, nullptr, 0);
        return static_cast<T>(v);
    }
    catch (...)
    {
        lg2::error("Invalid env {NAME}={VALUE}", "NAME", name, "VALUE", s);
        return std::nullopt;
    }
}

Max10Profile loadProfileFromEnv()
{
    Max10Profile p{};

    if (auto v = parseEnvUnsigned<uint32_t>("MAX10_CSR_BASE"))
        p.csrBase = *v;
    if (auto v = parseEnvUnsigned<uint32_t>("MAX10_DATA_BASE"))
        p.dataBase = *v;
    if (auto v = parseEnvUnsigned<uint32_t>("MAX10_BOOT_BASE"))
        p.bootBase = *v;
    if (auto v = parseEnvUnsigned<uint32_t>("MAX10_CFM_START"))
        p.startAddr = *v;
    if (auto v = parseEnvUnsigned<uint32_t>("MAX10_CFM_END"))
        p.endAddr = *v;
    if (auto v = parseEnvUnsigned<uint8_t>("MAX10_IMAGE_TYPE"))
        p.imageType = *v;
    if (auto v = parseEnvUnsigned<uint8_t>("MAX10_I2C_LITTLE_ENDIAN"))
    {
        p.littleEndian = (*v != 0);
    }

    return p;
}

} // namespace

std::unique_ptr<Max10CPLD> Max10CPLDFactory::getMax10CPLD() const
{
    const auto chipModelStr =
        getMax10ChipStr(chipEnum, max10StringType::modelString);
    if (chipModelStr.empty())
    {
        lg2::error("Unsupported MAX10 CPLD chip enum");
        return nullptr;
    }

    return std::make_unique<Max10CPLD>(ctx, bus, address, chipModelStr,
                                       chipModelStr, false,
                                       loadProfileFromEnv());
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
        const auto typeStr =
            getMax10ChipStr(chipEnum, max10StringType::typeString);

        CPLDFactory::instance().registerCPLD(
            typeStr, [chipEnum](sdbusplus::async::context& ctx,
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
