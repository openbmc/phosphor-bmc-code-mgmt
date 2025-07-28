#include "tpm2.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdio>
#include <regex>
#include <sstream>

PHOSPHOR_LOG2_USING;

static constexpr std::string_view getCapPropertiesCmd =
    "/usr/bin/tpm2_getcap properties-fixed";
static constexpr std::string_view fwVer1Property = "TPM2_PT_FIRMWARE_VERSION_1";
static constexpr std::string_view fwVer2Property = "TPM2_PT_FIRMWARE_VERSION_2";
static constexpr std::string_view manufacturerProperty = "TPM2_PT_MANUFACTURER";

static constexpr std::string_view hexPattern = R"(^\s*raw:\s+0x([0-9a-fA-F]+))";

static constexpr uint32_t nuvotonManufacturerId = 0x4E544300;

enum class Tpm2Vendor
{
    IFX,
    Nuvoton
};

sdbusplus::async::task<bool> TPM2Interface::getProperty(
    std::string_view property, uint32_t& value)
{
    // Reference: https://tpm2-tools.readthedocs.io/en/latest/man/common/tcti/
    // The TCTI or "Transmission Interface" is the communication mechanism
    // with the TPM. TCTIs can be changed for communication with TPMs across
    // different mediums.
    auto tcti = "device:" + getTPMResourceManagerPath();
    auto cmd = std::string(getCapPropertiesCmd) + " --tcti " + tcti +
               " | grep -A1 " + std::string(property);

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        error("Failed to run command: {CMD}", "CMD", cmd);
        co_return false;
    }

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        output += buffer;
    }
    pclose(pipe);

    const std::regex regexPattern{std::string(hexPattern)};
    std::smatch match;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        if (std::regex_search(line, match, regexPattern) && match.size() >= 2)
        {
            try
            {
                value = std::stoul(match[1].str(), nullptr, 16);
                co_return true;
            }
            catch (const std::exception& e)
            {
                error("Failed to parse hex value for property {PT}: {ERR}",
                      "PT", property, "ERR", e.what());
                co_return false;
            }
        }
    }

    error("No matching hex value found for property: {PT}", "PT", property);
    co_return false;
}

sdbusplus::async::task<bool> TPM2Interface::updateFirmware(const uint8_t* image,
                                                           size_t image_size)
{
    (void)image;
    (void)image_size;

    error("TPM2 firmware update is not supported");
    co_return false;
}

sdbusplus::async::task<bool> TPM2Interface::getVersion(std::string& version)
{
    uint32_t manufacturerId = 0;
    uint32_t fwVer = 0;
    std::string tpmVer1;
    std::string tpmVer2;
    Tpm2Vendor vendor = Tpm2Vendor::IFX;

    if (!co_await getProperty(manufacturerProperty, manufacturerId))
    {
        error("Failed to retrieve TPM manufacturer ID");
        co_return false;
    }

    if (manufacturerId == nuvotonManufacturerId)
    {
        vendor = Tpm2Vendor::Nuvoton;
    }

    if (!co_await getProperty(fwVer1Property, fwVer))
    {
        error("Failed to retrieve TPM firmware version 1");
        co_return false;
    }

    tpmVer1 = std::to_string(fwVer >> 16) + "." +
              std::to_string(fwVer & 0xFFFF);

    if (vendor == Tpm2Vendor::Nuvoton)
    {
        if (!co_await getProperty(fwVer2Property, fwVer))
        {
            error("Failed to retrieve TPM firmware version 2");
            co_return false;
        }

        tpmVer2 = std::to_string(fwVer >> 16) + "." +
                  std::to_string(fwVer & 0xFFFF);
        version = tpmVer1 + "." + tpmVer2;
    }
    else
    {
        version = tpmVer1;
    }

    co_return true;
}
