#include "utils.hpp"

#include <libpldm/pdr.h>
#include <libpldm/pldm_types.h>
#include <linux/mctp.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Logging/Create/client.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// NOLINTBEGIN

PHOSPHOR_LOG2_USING;

namespace pldm
{
namespace utils
{

uint8_t getNumPadBytes(uint32_t data)
{
    uint8_t pad;
    pad = ((data % 4) ? (4 - data % 4) : 0);
    return pad;
} // end getNumPadBytes

bool uintToDate(uint64_t data, uint16_t* year, uint8_t* month, uint8_t* day,
                uint8_t* hour, uint8_t* min, uint8_t* sec)
{
    constexpr uint64_t max_data = 29991231115959;
    constexpr uint64_t min_data = 19700101000000;
    if (data < min_data || data > max_data)
    {
        return false;
    }

    *year = data / 10000000000;
    data = data % 10000000000;
    *month = data / 100000000;
    data = data % 100000000;
    *day = data / 1000000;
    data = data % 1000000;
    *hour = data / 10000;
    data = data % 10000;
    *min = data / 100;
    *sec = data % 100;

    return true;
}

void printBuffer(bool isTx, const std::vector<uint8_t>& buffer)
{
    if (buffer.empty())
    {
        return;
    }

    std::cout << (isTx ? "Tx: " : "Rx: ");

    std::ranges::for_each(buffer, [](uint8_t byte) {
        std::cout << std::format("{:02x} ", byte);
    });

    std::cout << std::endl;
}

std::string toString(const struct variable_field& var)
{
    if (var.ptr == nullptr || !var.length)
    {
        return "";
    }

    std::string str(reinterpret_cast<const char*>(var.ptr), var.length);
    std::replace_if(
        str.begin(), str.end(), [](const char& c) { return !isprint(c); }, ' ');
    return str;
}

std::vector<std::string> split(std::string_view srcStr, std::string_view delim,
                               std::string_view trimStr)
{
    std::vector<std::string> out;
    size_t start;
    size_t end = 0;

    while ((start = srcStr.find_first_not_of(delim, end)) != std::string::npos)
    {
        end = srcStr.find(delim, start);
        std::string_view dstStr = srcStr.substr(start, end - start);
        if (!trimStr.empty())
        {
            dstStr.remove_prefix(dstStr.find_first_not_of(trimStr));
            dstStr.remove_suffix(
                dstStr.size() - 1 - dstStr.find_last_not_of(trimStr));
        }

        if (!dstStr.empty())
        {
            out.push_back(std::string(dstStr));
        }
    }

    return out;
}

std::string getCurrentSystemTime()
{
    const auto zonedTime{std::chrono::zoned_time{
        std::chrono::current_zone(), std::chrono::system_clock::now()}};
    return std::format("{:%F %Z %T}", zonedTime);
}

bool checkIfLogicalBitSet(const uint16_t& containerId)
{
    return !(containerId & 0x8000);
}

std::string_view trimNameForDbus(std::string& name)
{
    std::replace(name.begin(), name.end(), ' ', '_');
    auto nullTerminatorPos = name.find('\0');
    if (nullTerminatorPos != std::string::npos)
    {
        name.erase(nullTerminatorPos);
    }
    return name;
}

bool dbusPropValuesToDouble(const std::string_view& type,
                            const pldm::utils::PropertyValue& value,
                            double* doubleValue)
{
    if (!dbusValueNumericTypeNames.contains(type))
    {
        return false;
    }

    if (!doubleValue)
    {
        return false;
    }

    try
    {
        if (type == "uint8_t")
        {
            *doubleValue = static_cast<double>(std::get<uint8_t>(value));
        }
        else if (type == "int16_t")
        {
            *doubleValue = static_cast<double>(std::get<int16_t>(value));
        }
        else if (type == "uint16_t")
        {
            *doubleValue = static_cast<double>(std::get<uint16_t>(value));
        }
        else if (type == "int32_t")
        {
            *doubleValue = static_cast<double>(std::get<int32_t>(value));
        }
        else if (type == "uint32_t")
        {
            *doubleValue = static_cast<double>(std::get<uint32_t>(value));
        }
        else if (type == "int64_t")
        {
            *doubleValue = static_cast<double>(std::get<int64_t>(value));
        }
        else if (type == "uint64_t")
        {
            *doubleValue = static_cast<double>(std::get<uint64_t>(value));
        }
        else if (type == "double")
        {
            *doubleValue = static_cast<double>(std::get<double>(value));
        }
        else
        {
            return false;
        }
    }
    catch (const std::exception& e)
    {
        return false;
    }

    return true;
}
} // namespace utils
} // namespace pldm

// NOLINTEND
