#include <algorithm>
#include <sdbusplus/server.hpp>
#include <sdbusplus/exception.hpp>
#include <org/openbmc/Associations/server.hpp>

namespace sdbusplus
{
namespace org
{
namespace openbmc
{
namespace server
{

Associations::Associations(bus::bus& bus, const char* path)
        : _org_openbmc_Associations_interface(
                bus, path, _interface, _vtable, this)
{
}

Associations::Associations(bus::bus& bus, const char* path,
                           const std::map<std::string, PropertiesVariant>& vals)
        : Associations(bus, path)
{
    for (const auto& v : vals)
    {
        setPropertyByName(v.first, v.second);
    }
}



auto Associations::associations() const ->
        std::vector<std::tuple<std::string, std::string, std::string>>
{
    return _associations;
}

int Associations::_callback_get_associations(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(reply);
#ifndef DISABLE_TRANSACTION
        {
            auto tbus = m.get_bus();
            sdbusplus::server::transaction::Transaction t(tbus, m);
            sdbusplus::server::transaction::set_id
                (std::hash<sdbusplus::server::transaction::Transaction>{}(t));
        }
#endif

        auto o = static_cast<Associations*>(context);
        m.append(convertForMessage(o->associations()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Associations::associations(std::vector<std::tuple<std::string, std::string, std::string>> value) ->
        std::vector<std::tuple<std::string, std::string, std::string>>
{
    if (_associations != value)
    {
        _associations = value;
        _org_openbmc_Associations_interface.property_changed("associations");
    }

    return _associations;
}

int Associations::_callback_set_associations(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(value);
#ifndef DISABLE_TRANSACTION
        {
            auto tbus = m.get_bus();
            sdbusplus::server::transaction::Transaction t(tbus, m);
            sdbusplus::server::transaction::set_id
                (std::hash<sdbusplus::server::transaction::Transaction>{}(t));
        }
#endif

        auto o = static_cast<Associations*>(context);

        std::vector<std::tuple<std::string, std::string, std::string>> v{};
        m.read(v);
        o->associations(v);
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
namespace Associations
{
static const auto _property_associations =
    utility::tuple_to_array(message::types::type_id<
            std::vector<std::tuple<std::string, std::string, std::string>>>());
}
}

void Associations::setPropertyByName(const std::string& name,
                                     const PropertiesVariant& val)
{
    if (name == "associations")
    {
        auto& v = message::variant_ns::get<std::vector<std::tuple<std::string, std::string, std::string>>>(val);
        associations(v);
        return;
    }
}

auto Associations::getPropertyByName(const std::string& name) ->
        PropertiesVariant
{
    if (name == "associations")
    {
        return associations();
    }

    return PropertiesVariant();
}


const vtable::vtable_t Associations::_vtable[] = {
    vtable::start(),
    vtable::property("associations",
                     details::Associations::_property_associations
                        .data(),
                     _callback_get_associations,
                     _callback_set_associations,
                     vtable::property_::emits_change),
    vtable::end()
};

} // namespace server
} // namespace openbmc
} // namespace org
} // namespace sdbusplus

