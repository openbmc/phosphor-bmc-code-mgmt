#include "config.h"

#include <sdbusplus/server.hpp>

#include <fstream>
#include <map>
#include <string>

namespace utils
{

    enum {
        FAMILY = 0,
        YEAR,
        MONTH,
        DAY,
        MAJOR,
        MINOR,
        PERSONALITY,
        TOTAL
    };

    #define MAGIC_WORD        "startProperties"
    #define ROMFAMILY         "romFamily="
    #define ROMYEAR           "romYear="
    #define ROMMONTH          "romMonth="
    #define ROMDAY            "romDay="
    #define ROMMAJOR          "romMajor="
    #define ROMMINOR          "romMinor="
    #define ROMPERSONALITY    "personalities="

    #define PROPERTY_OFFSET   0x50000
    #define BUF_SIZE          0x1000      // 4K

    const std::map<int, std::string> _hostPropertyMap = {
        {FAMILY,      ROMFAMILY},
        {YEAR,        ROMYEAR},
        {MONTH,       ROMMONTH},
        {DAY,         ROMDAY},
        {MAJOR,       ROMMAJOR},
        {MINOR,       ROMMINOR},
        {PERSONALITY, ROMPERSONALITY}
    };

    /**
     * @brief Get Host FW version
     *
     * @return the FW version as a string
     **/
    std::string getHostVersion();

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
    template <typename T>
    T getProperty(sdbusplus::bus::bus& bus, const std::string& objectPath,
                const std::string& interface, const std::string& propertyName)
    {
        std::variant<T> value{};
        auto service = getService(bus, objectPath, interface);
        if (service.empty())
        {
            return std::get<T>(value);
        }

        auto method = bus.new_method_call(service.c_str(), objectPath.c_str(),
                                        "org.freedesktop.DBus.Properties", "Get");
        method.append(interface, propertyName);

        auto reply = bus.call(method);
        reply.read(value);

        return std::get<T>(value);
    }

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

    /**
     * @brief Flash erase MTD device
     *
     * @param[in] mtdName - MTD device name
     *
     * @return Result
    **/
    int flashEraseMTD(const std::string& mtdName);


} // namespace utils
