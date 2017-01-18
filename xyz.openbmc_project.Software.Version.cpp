#include <algorithm>
#include <sdbusplus/server.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

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

Version::Version(bus::bus& bus, const char* path)
        : _xyz_openbmc_project_Software_Version_interface(
                bus, path, _interface, _vtable, this)
{
}



auto Version::version() const ->
        std::string
{
    return _version;
}

int Version::_callback_get_Version(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<Version*>(context);
        m.append(convertForMessage(o->version()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Version::version(std::string value) ->
        std::string
{
    if (_version != value)
    {
        _version = value;
        _xyz_openbmc_project_Software_Version_interface.property_changed("Version");
    }

    return _version;
}

int Version::_callback_set_Version(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<Version*>(context);

        std::string v{};
        m.read(v);
        o->version(v);
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

namespace details
{
namespace Version
{
static const auto _property_Version =
    utility::tuple_to_array(message::types::type_id<
            std::string>());
}
}
auto Version::purpose() const ->
        VersionPurpose
{
    return _purpose;
}

int Version::_callback_get_Purpose(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<Version*>(context);
        m.append(convertForMessage(o->purpose()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Version::purpose(VersionPurpose value) ->
        VersionPurpose
{
    if (_purpose != value)
    {
        _purpose = value;
        _xyz_openbmc_project_Software_Version_interface.property_changed("Purpose");
    }

    return _purpose;
}

int Version::_callback_set_Purpose(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<Version*>(context);

        std::string v{};
        m.read(v);
        o->purpose(convertVersionPurposeFromString(v));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

namespace details
{
namespace Version
{
static const auto _property_Purpose =
    utility::tuple_to_array(message::types::type_id<
            std::string>());
}
}


namespace
{
/** String to enum mapping for Version::VersionPurpose */
static const std::tuple<const char*, Version::VersionPurpose> mappingVersionVersionPurpose[] =
        {
            std::make_tuple( "xyz.openbmc_project.Software.Version.VersionPurpose.Unknown",                 Version::VersionPurpose::Unknown ),
            std::make_tuple( "xyz.openbmc_project.Software.Version.VersionPurpose.Other",                 Version::VersionPurpose::Other ),
            std::make_tuple( "xyz.openbmc_project.Software.Version.VersionPurpose.System",                 Version::VersionPurpose::System ),
            std::make_tuple( "xyz.openbmc_project.Software.Version.VersionPurpose.BMC",                 Version::VersionPurpose::BMC ),
            std::make_tuple( "xyz.openbmc_project.Software.Version.VersionPurpose.Host",                 Version::VersionPurpose::Host ),
        };

} // anonymous namespace

auto Version::convertVersionPurposeFromString(std::string& s) ->
        VersionPurpose
{
    auto i = std::find_if(
            std::begin(mappingVersionVersionPurpose),
            std::end(mappingVersionVersionPurpose),
            [&s](auto& e){ return 0 == strcmp(s.c_str(), std::get<0>(e)); } );
    if (std::end(mappingVersionVersionPurpose) == i)
    {
        throw sdbusplus::exception::InvalidEnumString();
    }
    else
    {
        return std::get<1>(*i);
    }
}

std::string convertForMessage(Version::VersionPurpose v)
{
    auto i = std::find_if(
            std::begin(mappingVersionVersionPurpose),
            std::end(mappingVersionVersionPurpose),
            [v](auto& e){ return v == std::get<1>(e); });
    return std::get<0>(*i);
}

const vtable::vtable_t Version::_vtable[] = {
    vtable::start(),
    vtable::property("Version",
                     details::Version::_property_Version
                        .data(),
                     _callback_get_Version,
                     _callback_set_Version,
                     vtable::property_::emits_change),
    vtable::property("Purpose",
                     details::Version::_property_Purpose
                        .data(),
                     _callback_get_Purpose,
                     _callback_set_Purpose,
                     vtable::property_::emits_change),
    vtable::end()
};

} // namespace server
} // namespace Software
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus

