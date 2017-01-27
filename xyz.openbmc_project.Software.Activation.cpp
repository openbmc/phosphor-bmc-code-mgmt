#include <algorithm>
#include <sdbusplus/server.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

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

Activation::Activation(bus::bus& bus, const char* path)
        : _xyz_openbmc_project_Software_Activation_interface(
                bus, path, _interface, _vtable, this)
{
}



auto Activation::activation() const ->
        Activations
{
    return _activation;
}

int Activation::_callback_get_Activation(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<Activation*>(context);
        m.append(convertForMessage(o->activation()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Activation::activation(Activations value) ->
        Activations
{
    if (_activation != value)
    {
        _activation = value;
        _xyz_openbmc_project_Software_Activation_interface.property_changed("Activation");
    }

    return _activation;
}

int Activation::_callback_set_Activation(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<Activation*>(context);

        std::string v{};
        m.read(v);
        o->activation(convertActivationsFromString(v));
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
namespace Activation
{
static const auto _property_Activation =
    utility::tuple_to_array(message::types::type_id<
            std::string>());
}
}
auto Activation::requestedActivation() const ->
        RequestedActivations
{
    return _requestedActivation;
}

int Activation::_callback_get_RequestedActivation(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<Activation*>(context);
        m.append(convertForMessage(o->requestedActivation()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    if (_requestedActivation != value)
    {
        _requestedActivation = value;
        _xyz_openbmc_project_Software_Activation_interface.property_changed("RequestedActivation");
    }

    return _requestedActivation;
}

int Activation::_callback_set_RequestedActivation(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<Activation*>(context);

        std::string v{};
        m.read(v);
        o->requestedActivation(convertRequestedActivationsFromString(v));
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
namespace Activation
{
static const auto _property_RequestedActivation =
    utility::tuple_to_array(message::types::type_id<
            std::string>());
}
}


namespace
{
/** String to enum mapping for Activation::Activations */
static const std::tuple<const char*, Activation::Activations> mappingActivationActivations[] =
        {
            std::make_tuple( "xyz.openbmc_project.Software.Activation.Activations.NotReady",                 Activation::Activations::NotReady ),
            std::make_tuple( "xyz.openbmc_project.Software.Activation.Activations.Invalid",                 Activation::Activations::Invalid ),
            std::make_tuple( "xyz.openbmc_project.Software.Activation.Activations.Ready",                 Activation::Activations::Ready ),
            std::make_tuple( "xyz.openbmc_project.Software.Activation.Activations.Activating",                 Activation::Activations::Activating ),
            std::make_tuple( "xyz.openbmc_project.Software.Activation.Activations.Active",                 Activation::Activations::Active ),
            std::make_tuple( "xyz.openbmc_project.Software.Activation.Activations.Failed",                 Activation::Activations::Failed ),
        };

} // anonymous namespace

auto Activation::convertActivationsFromString(std::string& s) ->
        Activations
{
    auto i = std::find_if(
            std::begin(mappingActivationActivations),
            std::end(mappingActivationActivations),
            [&s](auto& e){ return 0 == strcmp(s.c_str(), std::get<0>(e)); } );
    if (std::end(mappingActivationActivations) == i)
    {
        throw sdbusplus::exception::InvalidEnumString();
    }
    else
    {
        return std::get<1>(*i);
    }
}

std::string convertForMessage(Activation::Activations v)
{
    auto i = std::find_if(
            std::begin(mappingActivationActivations),
            std::end(mappingActivationActivations),
            [v](auto& e){ return v == std::get<1>(e); });
    return std::get<0>(*i);
}

namespace
{
/** String to enum mapping for Activation::RequestedActivations */
static const std::tuple<const char*, Activation::RequestedActivations> mappingActivationRequestedActivations[] =
        {
            std::make_tuple( "xyz.openbmc_project.Software.Activation.RequestedActivations.None",                 Activation::RequestedActivations::None ),
            std::make_tuple( "xyz.openbmc_project.Software.Activation.RequestedActivations.Active",                 Activation::RequestedActivations::Active ),
        };

} // anonymous namespace

auto Activation::convertRequestedActivationsFromString(std::string& s) ->
        RequestedActivations
{
    auto i = std::find_if(
            std::begin(mappingActivationRequestedActivations),
            std::end(mappingActivationRequestedActivations),
            [&s](auto& e){ return 0 == strcmp(s.c_str(), std::get<0>(e)); } );
    if (std::end(mappingActivationRequestedActivations) == i)
    {
        throw sdbusplus::exception::InvalidEnumString();
    }
    else
    {
        return std::get<1>(*i);
    }
}

std::string convertForMessage(Activation::RequestedActivations v)
{
    auto i = std::find_if(
            std::begin(mappingActivationRequestedActivations),
            std::end(mappingActivationRequestedActivations),
            [v](auto& e){ return v == std::get<1>(e); });
    return std::get<0>(*i);
}

const vtable::vtable_t Activation::_vtable[] = {
    vtable::start(),
    vtable::property("Activation",
                     details::Activation::_property_Activation
                        .data(),
                     _callback_get_Activation,
                     _callback_set_Activation,
                     vtable::property_::emits_change),
    vtable::property("RequestedActivation",
                     details::Activation::_property_RequestedActivation
                        .data(),
                     _callback_get_RequestedActivation,
                     _callback_set_RequestedActivation,
                     vtable::property_::emits_change),
    vtable::end()
};

} // namespace server
} // namespace Software
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus

