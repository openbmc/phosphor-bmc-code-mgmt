#include "usb_manager.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>
#include <sdeventplus/event.hpp>

bool isEnabled()
{
    constexpr auto OBJECT_PATH =
        "/xyz/openbmc_project/control/service/phosphor_2dusb_2dcode_2dupdate";
    constexpr auto OBJECT_INTF =
        "xyz.openbmc_project.Control.Service.Attributes";
    constexpr auto PRONAME = "Enabled";

    try
    {
        auto value = phosphor::usb::utils::DBusHandler().getProperty(
            OBJECT_PATH, PRONAME, OBJECT_INTF);
        const auto& running = std::get<bool>(value);

        return running;
    }
    catch (const std::exception& e)
    {}

    return true;
}

int main(int argc, char** argv)
{
    // Verify that USB Code Update is disabled
    if (!isEnabled())
    {
        lg2::info("Disabled USB Code Update.");

        return 0;
    }

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
