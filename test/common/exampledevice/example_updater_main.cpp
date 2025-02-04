#include "example_device.hpp"

int main()
{
    sdbusplus::async::context ctx;

    ExampleCodeUpdater updater(ctx);

    updater.init();

    std::string service = "xyz.openbmc_project.ExampleCodeUpdater";
    ctx.get_bus().request_name(service.c_str());

    ctx.run();

    return 0;
}
