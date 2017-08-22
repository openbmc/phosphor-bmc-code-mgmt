#pragma once
#include <tuple>
#include <systemd/sd-bus.h>
#include <sdbusplus/server.hpp>

namespace sdbusplus
{
namespace org
{
namespace openbmc
{
namespace server
{

class Associations
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
        Associations() = delete;
        Associations(const Associations&) = delete;
        Associations& operator=(const Associations&) = delete;
        Associations(Associations&&) = delete;
        Associations& operator=(Associations&&) = delete;
        virtual ~Associations() = default;

        /** @brief Constructor to put object onto bus at a dbus path.
         *  @param[in] bus - Bus to attach to.
         *  @param[in] path - Path to attach at.
         */
        Associations(bus::bus& bus, const char* path);


        using PropertiesVariant = sdbusplus::message::variant<
                std::vector<std::tuple<std::string, std::string, std::string>>>;

        /** @brief Constructor to initialize the object from a map of
         *         properties.
         *
         *  @param[in] bus - Bus to attach to.
         *  @param[in] path - Path to attach at.
         *  @param[in] vals - Map of property name to value for initalization.
         */
        Associations(bus::bus& bus, const char* path,
                     const std::map<std::string, PropertiesVariant>& vals);



        /** Get value of associations */
        virtual std::vector<std::tuple<std::string, std::string, std::string>> associations() const;
        /** Set value of associations */
        virtual std::vector<std::tuple<std::string, std::string, std::string>> associations(std::vector<std::tuple<std::string, std::string, std::string>> value);

        /** @brief Sets a property by name.
         *  @param[in] name - A string representation of the property name.
         *  @param[in] val - A variant containing the value to set.
         */
        void setPropertyByName(const std::string& name,
                               const PropertiesVariant& val);

        /** @brief Gets a property by name.
         *  @param[in] name - A string representation of the property name.
         *  @return - A variant containing the value of the property.
         */
        PropertiesVariant getPropertyByName(const std::string& name);


    private:

        /** @brief sd-bus callback for get-property 'associations' */
        static int _callback_get_associations(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);
        /** @brief sd-bus callback for set-property 'associations' */
        static int _callback_set_associations(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);


        static constexpr auto _interface = "org.openbmc.Associations";
        static const vtable::vtable_t _vtable[];
        sdbusplus::server::interface::interface
                _org_openbmc_Associations_interface;

        std::vector<std::tuple<std::string, std::string, std::string>> _associations{};

};


} // namespace server
} // namespace openbmc
} // namespace org
} // namespace sdbusplus

