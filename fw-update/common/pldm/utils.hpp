#pragma once

#include "types.hpp"

#include <libpldm/base.h>
#include <libpldm/bios.h>
#include <libpldm/entity.h>
#include <libpldm/pdr.h>
#include <libpldm/platform.h>
#include <libpldm/utils.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Inventory/Manager/client.hpp>
#include <xyz/openbmc_project/Logging/Entry/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

// NOLINTBEGIN

namespace pldm
{
namespace utils
{

enum class Level
{
    WARNING,
    CRITICAL,
    PERFORMANCELOSS,
    SOFTSHUTDOWN,
    HARDSHUTDOWN,
    ERROR
};
enum class Direction
{
    HIGH,
    LOW,
    ERROR
};

const std::set<std::string_view> dbusValueTypeNames = {
    "bool",    "uint8_t",  "int16_t",         "uint16_t",
    "int32_t", "uint32_t", "int64_t",         "uint64_t",
    "double",  "string",   "vector<uint8_t>", "vector<string>"};
const std::set<std::string_view> dbusValueNumericTypeNames = {
    "uint8_t",  "int16_t", "uint16_t", "int32_t",
    "uint32_t", "int64_t", "uint64_t", "double"};

namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr bool Tx = true;
constexpr bool Rx = false;
using ObjectMapper = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>;
using inventoryManager =
    sdbusplus::client::xyz::openbmc_project::inventory::Manager<>;

constexpr auto dbusProperties = "org.freedesktop.DBus.Properties";
constexpr auto mapperService = ObjectMapper::default_service;
constexpr auto inventoryPath = "/xyz/openbmc_project/inventory";
/** @struct CustomFD
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFD
{
    CustomFD(const CustomFD&) = delete;
    CustomFD& operator=(const CustomFD&) = delete;
    CustomFD(CustomFD&&) = delete;
    CustomFD& operator=(CustomFD&&) = delete;

    CustomFD(int fd) : fd(fd) {}

    ~CustomFD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    int fd = -1;
};

/** @brief Calculate the pad for PLDM data
 *
 *  @param[in] data - Length of the data
 *  @return - uint8_t - number of pad bytes
 */
uint8_t getNumPadBytes(uint32_t data);

/** @brief Convert uint64 to date
 *
 *  @param[in] data - time date of uint64
 *  @param[out] year - year number in dec
 *  @param[out] month - month number in dec
 *  @param[out] day - day of the month in dec
 *  @param[out] hour - number of hours in dec
 *  @param[out] min - number of minutes in dec
 *  @param[out] sec - number of seconds in dec
 *  @return true if decode success, false if decode failed
 */
bool uintToDate(uint64_t data, uint16_t* year, uint8_t* month, uint8_t* day,
                uint8_t* hour, uint8_t* min, uint8_t* sec);

/** @brief Convert effecter data to structure of set_effecter_state_field
 *
 *  @param[in] effecterData - the date of effecter
 *  @param[in] effecterCount - the number of individual sets of effecter
 *                              information
 *  @return[out] parse success and get a valid set_effecter_state_field
 *               structure, return nullopt means parse failed
 */
std::optional<std::vector<set_effecter_state_field>> parseEffecterData(
    const std::vector<uint8_t>& effecterData, uint8_t effecterCount);

/**
 *  @brief creates an error log
 *  @param[in] errorMsg - the error message
 */
void reportError(const char* errorMsg);

/** @brief Convert any Decimal number to BCD
 *
 *  @tparam[in] decimal - Decimal number
 *  @return Corresponding BCD number
 */
template <typename T>
T decimalToBcd(T decimal)
{
    T bcd = 0;
    T rem = 0;
    auto cnt = 0;

    while (decimal)
    {
        rem = decimal % 10;
        bcd = bcd + (rem << cnt);
        decimal = decimal / 10;
        cnt += 4;
    }

    return bcd;
}

struct DBusMapping
{
    std::string objectPath;   //!< D-Bus object path
    std::string interface;    //!< D-Bus interface
    std::string propertyName; //!< D-Bus property name
    std::string propertyType; //!< D-Bus property type
};

using PropertyValue =
    std::variant<bool, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t,
                 uint64_t, double, std::string, std::vector<uint8_t>,
                 std::vector<std::string>>;
using DbusProp = std::string;
using DbusChangedProps = std::map<DbusProp, PropertyValue>;
using DBusInterfaceAdded = std::vector<
    std::pair<pldm::dbus::Interface,
              std::vector<std::pair<pldm::dbus::Property,
                                    std::variant<pldm::dbus::Property>>>>>;

using ObjectPath = std::string;
using EntityName = std::string;
using Entities = std::vector<pldm_entity_node*>;
using EntityAssociations = std::vector<Entities>;
using ObjectPathMaps = std::map<fs::path, pldm_entity_node*>;
using EntityMaps = std::map<pldm::pdr::EntityType, EntityName>;

using ServiceName = std::string;
using Interfaces = std::vector<std::string>;
using MapperServiceMap = std::vector<std::pair<ServiceName, Interfaces>>;
using GetSubTreeResponse = std::vector<std::pair<ObjectPath, MapperServiceMap>>;
using GetSubTreePathsResponse = std::vector<std::string>;
using PropertyMap = std::map<std::string, PropertyValue>;
using InterfaceMap = std::map<std::string, PropertyMap>;
using ObjectValueTree = std::map<sdbusplus::message::object_path, InterfaceMap>;

/** @brief Convert a value in the JSON to a D-Bus property value
 *
 *  @param[in] type - type of the D-Bus property
 *  @param[in] value - value in the JSON file
 *
 *  @return PropertyValue - the D-Bus property value
 */
PropertyValue jsonEntryToDbusVal(std::string_view type,
                                 const nlohmann::json& value);

/** @brief Print the buffer
 *
 *  @param[in]  isTx - True if the buffer is an outgoing PLDM message, false if
                       the buffer is an incoming PLDM message
 *  @param[in]  buffer - Buffer to print
 *
 *  @return - None
 */
void printBuffer(bool isTx, const std::vector<uint8_t>& buffer);

/** @brief Convert the buffer to std::string
 *
 *  If there are characters that are not printable characters, it is replaced
 *  with space(0x20).
 *
 *  @param[in] var - pointer to data and length of the data
 *
 *  @return std::string equivalent of variable field
 */
std::string toString(const struct variable_field& var);

/** @brief Split strings according to special identifiers
 *
 *  We can split the string according to the custom identifier(';', ',', '&' or
 *  others) and store it to vector.
 *
 *  @param[in] srcStr       - The string to be split
 *  @param[in] delim        - The custom identifier
 *  @param[in] trimStr      - The first and last string to be trimmed
 *
 *  @return std::vector<std::string> Vectors are used to store strings
 */
std::vector<std::string> split(std::string_view srcStr, std::string_view delim,
                               std::string_view trimStr = "");
/** @brief Get the current system time in readable format
 *
 *  @return - std::string equivalent of the system time
 */
std::string getCurrentSystemTime();

} // namespace utils
} // namespace pldm

// NOLINTEND
