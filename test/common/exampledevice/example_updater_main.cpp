#include "example_device.hpp"

#include <sdbusplus/async/context.hpp>

using namespace phosphor::software::example_device;

sdbusplus::async::task<void> init(ExampleCodeUpdater& updater)
{
    /*
     * In Concrete updaters, the initDevices() function needs to be called,
     * which in turn invokes the virtual initDevice() function implemented here.
     * However, in ExampleUpdater, the initDevice() function is called directly
     * because there is no example configuration from EM to consume, which would
     * otherwise cause the initDevices() API to throw an error. Therefore,
     * calling initDevice() directly in this case.
     */

    co_await updater.initDevice("", "", ExampleDevice::defaultConfig);

    co_return;
}

int main()
{
    sdbusplus::async::context ctx;

    ExampleCodeUpdater updater(ctx);

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(init(updater));

    std::string busName = "xyz.openbmc_project.Software.ExampleDevice";
    ctx.get_bus().request_name(busName.c_str());

    ctx.run();

    return 0;
}
