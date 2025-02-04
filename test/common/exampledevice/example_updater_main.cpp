#include "example_device.hpp"

using namespace phosphor::software::example_device;

int main()
{
    sdbusplus::async::context ctx;

    ExampleCodeUpdater updater(ctx);

    /*
     * In Concrete updaters, the initDevices() function needs to be called,
     * which in turn invokes the virtual initDevice() function implemented here.
     * However, in ExampleUpdater, the initDevice() function is called directly
     * because there is no example configuration from EM to consume, which would
     * otherwise cause the initDevices() API to throw an error. Therefore,
     * calling initDevice() directly in this case.
     */

    ctx.spawn(updater.initDevice("", "", ExampleDevice::defaultConfig));

    std::string busName = "xyz.openbmc_project.Software.ExampleDevice";
    ctx.get_bus().request_name(busName.c_str());

    ctx.run();

    return 0;
}
