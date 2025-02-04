#include "example_device.hpp"

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

    std::string service = "xyz.openbmc_project.ExampleCodeUpdater";
    ctx.get_bus().request_name(service.c_str());

    ctx.run();

    return 0;
}
