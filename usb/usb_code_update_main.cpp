#include "usb_manager.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/lg2.hpp>

int main(int argc, char** argv)
{
    std::string fileName{};

    CLI::App app{"Update the firmware of OpenBMC via USB app"};
    app.add_option("-f,--fileName", fileName,
                   "Get the name of the USB mount folder");

    CLI11_PARSE(app, argc, argv);

    std::string usbPath = "/media/" + fileName;
    phosphor::usb::USBManager manager(usbPath.c_str());

    if (!manager.run())
    {
        lg2::error("Failed to FW Update via USB, usbPath:{USBPATH}", "USBPATH",
                   usbPath);
        return -1;
    }

    return 0;
}
