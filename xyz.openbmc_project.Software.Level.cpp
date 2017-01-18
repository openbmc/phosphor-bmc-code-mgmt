#include <algorithm>
#include <sdbusplus/server.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Software/Level/server.hpp>

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

Level::Level(bus::bus& bus, const char* path)
        : _xyz_openbmc_project_Software_Level_interface(
                bus, path, _interface, _vtable, this)
{
}



auto Level::version() const ->
        std::string
{
    return _version;
}

int Level::_callback_get_Version(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<Level*>(context);
        m.append(convertForMessage(o->version()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Level::version(std::string value) ->
        std::string
{
    if (_version != value)
    {
        _version = value;
        _xyz_openbmc_project_Software_Level_interface.property_changed("Version");
    }

    return _version;
}

int Level::_callback_set_Version(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<Level*>(context);

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
namespace Level
{
static const auto _property_Version =
    utility::tuple_to_array(message::types::type_id<
            std::string>());
}
}
auto Level::purpose() const ->
        LevelPurpose
{
    return _purpose;
}

int Level::_callback_get_Purpose(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<Level*>(context);
        m.append(convertForMessage(o->purpose()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Level::purpose(LevelPurpose value) ->
        LevelPurpose
{
    if (_purpose != value)
    {
        _purpose = value;
        _xyz_openbmc_project_Software_Level_interface.property_changed("Purpose");
    }

    return _purpose;
}

int Level::_callback_set_Purpose(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<Level*>(context);

        std::string v{};
        m.read(v);
        o->purpose(convertLevelPurposeFromString(v));
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
namespace Level
{
static const auto _property_Purpose =
    utility::tuple_to_array(message::types::type_id<
            std::string>());
}
}


namespace
{
/** String to enum mapping for Level::LevelPurpose */
static const std::tuple<const char*, Level::LevelPurpose> mappingLevelLevelPurpose[] =
        {
            std::make_tuple( "xyz.openbmc_project.Software.Level.LevelPurpose.Unknown",                 Level::LevelPurpose::Unknown ),
            std::make_tuple( "xyz.openbmc_project.Software.Level.LevelPurpose.Other",                 Level::LevelPurpose::Other ),
            std::make_tuple( "xyz.openbmc_project.Software.Level.LevelPurpose.System",                 Level::LevelPurpose::System ),
            std::make_tuple( "xyz.openbmc_project.Software.Level.LevelPurpose.BMC",                 Level::LevelPurpose::BMC ),
            std::make_tuple( "xyz.openbmc_project.Software.Level.LevelPurpose.Host",                 Level::LevelPurpose::Host ),
        };

} // anonymous namespace

auto Level::convertLevelPurposeFromString(std::string& s) ->
        LevelPurpose
{
    auto i = std::find_if(
            std::begin(mappingLevelLevelPurpose),
            std::end(mappingLevelLevelPurpose),
            [&s](auto& e){ return 0 == strcmp(s.c_str(), std::get<0>(e)); } );
    if (std::end(mappingLevelLevelPurpose) == i)
    {
        throw sdbusplus::exception::InvalidEnumString();
    }
    else
    {
        return std::get<1>(*i);
    }
}

std::string convertForMessage(Level::LevelPurpose v)
{
    auto i = std::find_if(
            std::begin(mappingLevelLevelPurpose),
            std::end(mappingLevelLevelPurpose),
            [v](auto& e){ return v == std::get<1>(e); });
    return std::get<0>(*i);
}

const vtable::vtable_t Level::_vtable[] = {
    vtable::start(),
    vtable::property("Version",
                     details::Level::_property_Version
                        .data(),
                     _callback_get_Version,
                     _callback_set_Version,
                     vtable::property_::emits_change),
    vtable::property("Purpose",
                     details::Level::_property_Purpose
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

