#include "usb_manager.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

int main(int argc, char** argv)
{
    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    /** @brief Dbus constructs */
    auto& bus = phosphor::usb::utils::DBusHandler::getBus();

    std::string fileName{};

    CLI::App app{"Update the firmware of OpenBMC via USB app"};
    app.add_option("-f,--fileName", fileName,
                   "Get the name of the USB mount folder");

    CLI11_PARSE(app, argc, argv);

    std::string usbPath = "/media/" + fileName;
    phosphor::usb::USBManager manager(event, usbPath.c_str());

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    event.loop();

    return 0;
}
