
#include "vr_code_updater.hpp"

int main()
{
    sdbusplus::async::context io;

    VRCodeUpdater vrcu(io, true);

    io.run();
    return 0;
}
