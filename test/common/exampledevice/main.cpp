#include "example_device.hpp"

#include <memory>

int main()
{
    sdbusplus::async::context ctx;

    ExampleCodeUpdater examplecu(ctx);
    ExampleCodeUpdater* cu = &examplecu;

    auto device = std::make_unique<ExampleDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    device->softwareCurrent->setVersion("v1.0");

    auto applyTimes = {RequestedApplyTimes::OnReset};

    device->softwareCurrent->enableUpdate(applyTimes);

    cu->devices.insert(std::move(device));

    std::string service = "xyz.openbmc_project.ExampleCodeUpdater";
    ctx.get_bus().request_name(service.c_str());

    ctx.run();

    return 0;
}
