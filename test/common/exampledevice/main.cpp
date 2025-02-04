#include "example_device.hpp"

#include <memory>

int main()
{
    sdbusplus::async::context ctx;

    ExampleCodeUpdater examplecu(ctx);
    ExampleCodeUpdater* cu = &examplecu;

    cu->init();

    std::string service = "xyz.openbmc_project.ExampleCodeUpdater";
    ctx.get_bus().request_name(service.c_str());

    ctx.run();

    return 0;
}
