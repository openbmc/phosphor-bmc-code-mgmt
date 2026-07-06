#include "spi_software_manager.hpp"

#include <sdbusplus/async.hpp>

using namespace phosphor::software::manager;

int main(int argc, char* argv[])
{
    bool dryRun = false;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--dryrun")
        {
            dryRun = true;
        }
    }

    sdbusplus::async::context ctx;

    SPISoftwareManager spiSoftwareManager(ctx, dryRun);

    spiSoftwareManager.start();

    return 0;
}
