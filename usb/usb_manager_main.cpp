#include "usb_manager.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;

    std::string deviceName{};

    CLI::App app{"Update the firmware of OpenBMC via USB app"};
    app.add_option("-d,--device", deviceName,
                   "Get the name of the USB device name, eg: sda1, sdb1");

    CLI11_PARSE(app, argc, argv);

    if (deviceName.empty())
    {
        lg2::error("The file name passed in is empty.");
        return -1;
    }

    fs::path devicePath = fs::path{"/dev"} / deviceName;
    fs::path usbPath = fs::path{"/run/media/usb"} / deviceName;

#ifdef START_UPDATE_DBUS_INTEFACE

    sdbusplus::async::context ctx;
    phosphor::usb::USBManager manager(ctx, devicePath, usbPath);
    ctx.run();

#else

    // Dbus constructs
    auto bus = sdbusplus::bus::new_default();

    // Get a default event loop
    auto event = sdeventplus::Event::get_default();

    phosphor::usb::USBManager manager(bus, event, devicePath, usbPath);

    // Attach the bus to sd_event to service user requests
    bus.attach_event(event.get(), SD_EVENT_PRIORITY_NORMAL);
    event.loop();

#endif // START_UPDATE_DBUS_INTEFACE

    return 0;
}
