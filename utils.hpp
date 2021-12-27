#include "config.h"

#include <sdbusplus/server.hpp>

#include <fstream>
#include <map>
#include <string>

namespace utils
{

using PropertyValue = std::variant<std::string>;

/**
 * @brief Get the bus service
 *
 * @return the bus service as a string
 **/
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

/** @brief Get property(type: variant)
 *
 *  @param[in] bus              -   bus handler
 *  @param[in] objectPath       -   D-Bus object path
 *  @param[in] interface        -   D-Bus interface
 *  @param[in] propertyName     -   D-Bus property name
 *
 *  @return The value of the property(type: variant)
 *
 *  @throw sdbusplus::exception::exception when it fails
 */
const PropertyValue getProperty(sdbusplus::bus::bus& bus,
                                const std::string& objectPath,
                                const std::string& interface,
                                const std::string& propertyName);

/** @brief Set D-Bus property
 *
 *  @param[in] bus              -   bus handler
 *  @param[in] objectPath       -   D-Bus object path
 *  @param[in] interface        -   D-Bus interface
 *  @param[in] propertyName     -   D-Bus property name
 *  @param[in] value            -   The value to be set
 *
 *  @throw sdbusplus::exception::exception when it fails
 */
void setProperty(sdbusplus::bus::bus& bus, const std::string& objectPath,
                 const std::string& interface, const std::string& propertyName,
                 const PropertyValue& value);

/**
 * @brief Merge more files
 *
 * @param[in] srcFiles - source files
 * @param[out] dstFile - destination file
 * @return
 **/
void mergeFiles(const std::vector<std::string>& srcFiles,
                const std::string& dstFile);

namespace internal
{

/**
 * @brief Construct an argument vector to be used with an exec command, which
 *        requires the name of the executable to be the first argument, and a
 *        null terminator to be the last.
 * @param[in] name - Name of the executable
 * @param[in] args - Optional arguments
 * @return char* vector
 */
template <typename... Arg>
constexpr auto constructArgv(const char* name, Arg&&... args)
{
    std::vector<char*> argV{
        {const_cast<char*>(name), const_cast<char*>(args)..., nullptr}};
    return argV;
}

/**
 * @brief Helper function to execute command in child process
 * @param[in] path - Fully qualified name of the executable to run
 * @param[in] args - Optional arguments
 * @return 0 on success
 */
int executeCmd(const char* path, char** args);

} // namespace internal

/**
 * @brief Execute command in child process
 * @param[in] path - Fully qualified name of the executable to run
 * @param[in] args - Optional arguments
 * @return 0 on success
 */
template <typename... Arg>
int execute(const char* path, Arg&&... args)
{
    auto argArray = internal::constructArgv(path, std::forward<Arg>(args)...);

    return internal::executeCmd(path, argArray.data());
}

} // namespace utils
