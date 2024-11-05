#include "i2c_vr_device_code_updater.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

#include <cstring>
#include <fstream>
#include <iostream>

using namespace std;

void printHelpText()
{
    cout << "--help				: print help" << endl;
    cout << "--manual			: start manual update" << endl;
    cout << "--image <filename>		: filename for manual update" << endl;
    cout << "--dryrun" << endl;
}

// NOLINTNEXTLINE
sdbusplus::async::task<> startManualUpdate(sdbusplus::async::context& ctx,
                                           I2CVRDeviceCodeUpdater& i2cdcu,
                                           const std::string& imageFilename)
{
    if (i2cdcu.devices.empty())
    {
        lg2::error("no device available for manual update");
        co_return;
    }

    const std::unique_ptr<Device>& device = *i2cdcu.devices.begin();

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

    auto sap = std::make_unique<SoftwareActivationProgress>(
        ctx, "/dummyActivationI2C");

    co_await device->deviceSpecificUpdateFunction(buffer.get(), size, sap);

    co_return;
}

// NOLINTNEXTLINE
sdbusplus::async::task<> start(sdbusplus::async::context& ctx,
                               I2CVRDeviceCodeUpdater& i2cdcu, bool manual,
                               const std::string& imageFilename)
{
    co_await i2cdcu.getInitialConfiguration();

    if (manual)
    {
        lg2::info("Manual Udpdate start");
        co_await startManualUpdate(ctx, i2cdcu, imageFilename);
    }

    co_return;
}

int main(int argc, char* argv[])
{
    // NOLINTBEGIN
    bool manualUpdate = false;
    bool printHelp = false;
    bool dryRun = false;
    std::string imageFile = "";

    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--manual")
        {
            manualUpdate = true;
        }
        if (arg == "--help")
        {
            printHelp = true;
        }
        if (arg == "--dryrun")
        {
            dryRun = true;
        }
        if (arg == "--image" && i < argc - 1)
        {
            imageFile = std::string(argv[i + 1]);
            i++;
        }
    }

    if (printHelp)
    {
        printHelpText();
        return 0;
    }

    sdbusplus::async::context ctx;
    I2CVRDeviceCodeUpdater i2cdcu(ctx, dryRun);

    ctx.spawn(start(ctx, i2cdcu, manualUpdate, imageFile));

    ctx.run();

    // NOLINTEND
    return 0;
}