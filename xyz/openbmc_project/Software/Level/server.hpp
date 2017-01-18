#pragma once
#include <tuple>
#include <systemd/sd-bus.h>
#include <sdbusplus/server.hpp>

namespace sdbusplus
{
namespace xyz
{
namespace openbmc_project
{
namespace Software
{
namespace server
{

class Level
{
    public:
        /* Define all of the basic class operations:
         *     Not allowed:
         *         - Default constructor to avoid nullptrs.
         *         - Copy operations due to internal unique_ptr.
         *         - Move operations due to 'this' being registered as the
         *           'context' with sdbus.
         *     Allowed:
         *         - Destructor.
         */
        Level() = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;
        Level(Level&&) = delete;
        Level& operator=(Level&&) = delete;
        virtual ~Level() = default;

        /** @brief Constructor to put object onto bus at a dbus path.
         *  @param[in] bus - Bus to attach to.
         *  @param[in] path - Path to attach at.
         */
        Level(bus::bus& bus, const char* path);

        enum class LevelPurpose
        {
            Unknown,
            Other,
            System,
            BMC,
            Host,
        };



        /** Get value of Version */
        virtual std::string version() const;
        /** Set value of Version */
        virtual std::string version(std::string value);
        /** Get value of Purpose */
        virtual LevelPurpose purpose() const;
        /** Set value of Purpose */
        virtual LevelPurpose purpose(LevelPurpose value);

    /** @brief Convert a string to an appropriate enum value.
     *  @param[in] s - The string to convert in the form of
     *                 "xyz.openbmc_project.Software.Level.<value name>"
     *  @return - The enum value.
     */
    static LevelPurpose convertLevelPurposeFromString(std::string& s);

    private:

        /** @brief sd-bus callback for get-property 'Version' */
        static int _callback_get_Version(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);
        /** @brief sd-bus callback for set-property 'Version' */
        static int _callback_set_Version(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);

        /** @brief sd-bus callback for get-property 'Purpose' */
        static int _callback_get_Purpose(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);
        /** @brief sd-bus callback for set-property 'Purpose' */
        static int _callback_set_Purpose(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);


        static constexpr auto _interface = "xyz.openbmc_project.Software.Level";
        static const vtable::vtable_t _vtable[];
        sdbusplus::server::interface::interface
                _xyz_openbmc_project_Software_Level_interface;

        std::string _version{};
        LevelPurpose _purpose{};

};

/* Specialization of sdbusplus::server::bindings::details::convertForMessage
 * for enum-type Level::LevelPurpose.
 *
 * This converts from the enum to a constant c-string representing the enum.
 *
 * @param[in] e - Enum value to convert.
 * @return C-string representing the name for the enum value.
 */
std::string convertForMessage(Level::LevelPurpose e);

} // namespace server
} // namespace Software
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus

