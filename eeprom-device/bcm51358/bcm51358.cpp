#include "bcm51358.hpp"

std::string BCM51358DeviceVersion::getVersion()
{
    std::string version = "Unknown";

    executeCmd("v", "Version:", [&version](const std::string_view& ver) {
        version = lstrip(ver);
        return true;
    });

    return version;
}
