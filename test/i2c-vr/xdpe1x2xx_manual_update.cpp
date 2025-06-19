#include "xdpe1x2xx/xdpe1x2xx.hpp"

#include <fstream>
#include <iostream>

using namespace phosphor::software::VR;

/* This manual update program is intended for testing
 * VR fw update outside of a full BMC environment, such
 * as when using the VR on a breakout board.
 *
 * We bypass Redfish, DBus and PLDM packaging and only
 * go through the device specific code with the plain VR fw image.
 */

sdbusplus::async::task<void> printCRC(XDPE1X2XX& xdpe)
{
    uint32_t sum = 0;
    co_await xdpe.getCRC(&sum);
    std::cout << "CRC sum: " << std::hex << sum << std::dec << std::endl;
}

sdbusplus::async::task<void> doUpdate(XDPE1X2XX& xdpe, const uint8_t* image,
                                      size_t imageSize)
{
    if (!co_await xdpe.verifyImage(image, imageSize))
    {
        std::cerr << "could not verify fw image" << std::endl;
        co_return;
    }

    if (!co_await xdpe.updateFirmware(false))
    {
        std::cerr << "could not update fw image" << std::endl;
        co_return;
    }

    if (!co_await xdpe.reset())
    {
        std::cerr << "could not reset" << std::endl;
        co_return;
    }

    co_await printCRC(xdpe);
}

sdbusplus::async::task<void> run(sdbusplus::async::context& ctx,
                                 std::string& arg)
{
    const uint16_t bus = 1;

    // The address is fixed on the breakout board
    const uint16_t address = 0x58;

    XDPE1X2XX xdpe(ctx, bus, address);

    co_await printCRC(xdpe);

    const bool crcOnly = (arg == "crc");

    if (crcOnly)
    {
        co_return;
    }

    const std::string& filename = arg;

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Could not open file: " << filename << std::endl;
        co_return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto buffer = std::vector<uint8_t>(size);

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        std::cerr << "Error reading file: " << filename << std::endl;
        co_return;
    }

    co_await doUpdate(xdpe, buffer.data(), size);
}

void help()
{
    std::cout << "Usage to get CRC:" << std::endl;
    std::cout << "./xdpe1x2xx-manual-update crc" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage to update:" << std::endl;
    std::cout << "./xdpe1x2xx-manual-update vr_firmware.mic" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        help();
        return 1;
    }
    std::string arg = std::string(argv[1]);

    if (arg == "help" || arg == "-help" || arg == "--help")
    {
        help();
        return 0;
    }

    sdbusplus::async::context ctx;

    ctx.spawn(run(ctx, arg));

    ctx.request_stop();

    ctx.run();

    return 0;
}
