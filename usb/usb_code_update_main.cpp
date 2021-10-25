#include "config.h"

#include "usb_manager.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/lg2.hpp>

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

    fs::path usbPath = MEDIA_DIR / fs::path{"run"} / fileName;
    phosphor::usb::USBManager manager(usbPath);

    if (!manager.run())
    {
        lg2::error("Failed to FW Update via USB, usbPath:{USBPATH}", "USBPATH",
                   usbPath);
        return -1;
    }

    return 0;
}
