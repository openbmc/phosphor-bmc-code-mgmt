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
    std::cout << "--help				: print help" << std::endl;
    std::cout << "--manual				: start manual update" << std::endl;
    std::cout << "--image <filename>	: filename for manual update"
              << std::endl;
}

// NOLINTNEXTLINE
sdbusplus::async::task<> startManualUpdate(sdbusplus::async::context& io,
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

    auto sap =
        std::make_unique<SoftwareActivationProgress>(io, "/dummyActivationI2C");

    co_await device->deviceSpecificUpdateFunction(buffer.get(), size, sap);

    co_return;
}

// NOLINTNEXTLINE
sdbusplus::async::task<> start(sdbusplus::async::context& io,
                               I2CVRDeviceCodeUpdater& i2cdcu, bool manual,
                               const std::string& imageFilename)
{
    co_await i2cdcu.getInitialConfiguration();

    if (manual)
    {
        co_await startManualUpdate(io, i2cdcu, imageFilename);
    }

    co_return;
}

int main(int argc, char* argv[])
{
    bool manualUpdate = false;
    bool printHelp = false;
    bool dryRun = false;
    bool debug = false;
    std::string imageFile = "";

    for (int i = 1; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--manual"))
        {
            manualUpdate = true;
        }
        else if (!std::strcmp(argv[i], "--help"))
        {
            printHelp = true;
        }
        else if (!std::strcmp(argv[i], "--dryrun"))
        {
            dryRun = true;
        }
        else if (!std::strcmp(argv[i], "--debug"))
        {
            debug = true;
        }
        else if (!std::strcmp(argv[i], "--image"))
        {
            imageFile = std::string(argv[i + 1]);
            i++;
        }
        else
        {
            return -1;
        }
    }

    if (printHelp)
    {
        printHelpText();
        return 0;
    }

    if (manualUpdate)
    {
        if (std::strcmp(imageFile.c_str(), ""))
        {
            return -1;
        }
    }

    sdbusplus::async::context io;
    I2CVRDeviceCodeUpdater i2cdcu(io, dryRun, debug);

    io.spawn(i2cdcu.getInitialConfiguration());

    io.run();
    return 0;
}
