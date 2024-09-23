#include "spi_device_code_updater.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

#include <fstream>
#include <iostream>

// implementing the design
// https://github.com/openbmc/docs/blob/377ed14df4913010752ee2faff994a50e12a6316/designs/code-update.md

// NOLINTNEXTLINE
sdbusplus::async::task<> startManualUpdate(sdbusplus::async::context& io,
                                           SPIDeviceCodeUpdater& spidcu,
                                           const std::string& imageFilename)
{
    if (spidcu.devices.empty())
    {
        lg2::error("no device available for manual update");
        co_return;
    }

    const std::unique_ptr<Device>& device = *spidcu.devices.begin();

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

    // TODO: find the proper object path here
    auto sap =
        std::make_unique<SoftwareActivationProgress>(io, "/dummyActivation");

    co_await device->deviceSpecificUpdateFunction(buffer.get(), size, sap);

    co_return;
}

// NOLINTNEXTLINE
sdbusplus::async::task<> start(sdbusplus::async::context& io,
                               SPIDeviceCodeUpdater& spidcu, bool manual,
                               const std::string& imageFilename)
{
    co_await spidcu.getInitialConfiguration();

    if (manual)
    {
        co_await startManualUpdate(io, spidcu, imageFilename);
    }

    co_return;
}

void printHelpText()
{
    std::cout << "--help              : print help" << std::endl;
    std::cout << "--manual            : start a manual update" << std::endl;
    std::cout << "--image <filename>  : filename for manual update"
              << std::endl;
}

int main(int argc, char* argv[])
{
    // getting a really unspecific error from clang-tidy here
    // about an uninitialized / garbage branch. Happy to discuss.

    // NOLINTBEGIN

    sdbusplus::async::context io;

    bool manualUpdate = false;
    bool printHelp = false;
    bool dryRun = false;
    bool debug = false;
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
        if (arg == "-debug")
        {
            debug = true;
        }
    }

    if (printHelp)
    {
        printHelpText();
    }

    SPIDeviceCodeUpdater spidcu(io, dryRun, debug);

    io.spawn(start(io, spidcu, manualUpdate, imageFilename));

    io.run();

    // NOLINTEND

    return 0;
}
