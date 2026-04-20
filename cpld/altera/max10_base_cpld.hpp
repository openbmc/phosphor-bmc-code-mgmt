#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace phosphor::software::cpld
{

enum class max10Chip
{
    MAX10_10M04,
    MAX10_10M08,
    MAX10_10M16,
    MAX10_10M25,
    UNSUPPORTED = -1,
};

enum class max10StringType
{
    typeString,
    modelString,
};

enum class max10ChipFamily
{
    MAX10,
};

struct Max10CpldInfo
{
    max10ChipFamily chipFamily;
    std::vector<uint8_t> deviceId;
};

std::string getMax10ChipStr(max10Chip chip, max10StringType stringType);

const std::map<max10Chip, Max10CpldInfo>& getSupportedDeviceMap();

} // namespace phosphor::software::cpld
