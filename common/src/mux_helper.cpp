#include "mux_helper.hpp"

#include "dbus_helper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>

#include <optional>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

sdbusplus::async::task<std::tuple<std::vector<std::string>, std::vector<bool>>>
    getMuxGPIOs(sdbusplus::async::context& ctx, const std::string service,
                const sdbusplus::object_path path,
                const std::string configIface)
{
    std::vector<std::string> gpioLines;
    std::vector<bool> gpioPolarities;

    const std::string configIfaceMux = configIface + ".MuxOutputs";

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        std::optional<std::string> name =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Name");

        std::optional<std::string> polarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Polarity");

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        gpioLines.push_back(name.value());
        gpioPolarities.push_back(polarity.value() == "High");
    }

    co_return {gpioLines, gpioPolarities};
}
