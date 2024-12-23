#include "eeprom_device_code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/async.hpp>

#include <fstream>
#include <iostream>

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> startManualUpdate(sdbusplus::async::context& ctx,
                                           EEPROMDeviceCodeUpdater& eepromdcu,
                                           const std::string& imageFilename)
{
    (void)ctx;
    if (eepromdcu.devices.empty())
    {
        lg2::error("no device available for manual update");
        co_return;
    }

    const std::unique_ptr<Device>& device = eepromdcu.devices.begin()->second;

    std::ifstream file(imageFilename, std::ios::binary | std::ios::ate);

    if (!file.good())
    {
        lg2::error("error opening file {FILENAME}", "FILENAME", imageFilename);
        co_return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto buffer = std::make_unique<uint8_t[]>(size);

    if (!file.read(reinterpret_cast<char*>(buffer.get()), size))
    {
        throw std::runtime_error("Error reading file: " + imageFilename);
    }

    co_await device->updateDevice(buffer.get(), size);

    co_return;
}

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<> start(sdbusplus::async::context& ctx,
                               EEPROMDeviceCodeUpdater& eepromdcu, bool manual,
                               const std::string& imageFilename)
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration." + configTypeEEPROMDevice};

    co_await eepromdcu.initDevices(configIntfs);

    if (manual)
    {
        co_await startManualUpdate(ctx, eepromdcu, imageFilename);
    }

    co_return;
}

void printHelpText()
{
    std::cout << "--help              : print help" << std::endl;
    std::cout << "--manual            : start a manual update" << std::endl;
    std::cout << "--dryrun            : dry run" << std::endl;
    std::cout << "--image <filename>  : filename for manual update"
              << std::endl;
}

int main(int argc, char* argv[])
{
    // NOLINTBEGIN
    sdbusplus::async::context ctx;

    bool manualUpdate = false;
    bool printHelp = false;
    bool dryRun = false;
    std::string imageFilename = "";

    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--manual")
        {
            manualUpdate = true;
        }
        if (arg == "--image" && i < argc - 1)
        {
            imageFilename = std::string(argv[i + 1]);
            i++;
        }
        if (arg == "--help")
        {
            printHelp = true;
        }
        if (arg == "--dryrun")
        {
            dryRun = true;
        }
    }

    if (printHelp)
    {
        printHelpText();
    }

    EEPROMDeviceCodeUpdater eepromdcu(ctx, dryRun);

    ctx.spawn(start(ctx, eepromdcu, manualUpdate, imageFilename));

    ctx.run();

    // NOLINTEND

    return 0;
}
