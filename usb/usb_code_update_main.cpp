#include "config.h"

#include "usb_manager.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    std::string fileName{};

    CLI::App app{"Update the firmware of OpenBMC via USB app"};
    app.add_option("-f,--fileName", fileName,
                   "Get the name of the USB mount folder, eg: sda1, sdb1");

    CLI11_PARSE(app, argc, argv);

    if (fileName.empty())
    {
        lg2::error("The file name passed in is empty.");
        return -1;
    }

    // Dbus constructs
    auto bus = sdbusplus::bus::new_default();

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    fs::path usbPath = MEDIA_DIR / fs::path{"run"} / fileName;
    phosphor::usb::USBManager manager(bus, event, usbPath);

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    event.loop();

    return 0;
}
