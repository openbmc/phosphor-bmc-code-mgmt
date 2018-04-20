#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include "config.h"
#include "sync_manager.hpp"

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);
    phosphor::software::manager::Sync syncManager();
    bus.request_name(SYNC_BUSNAME);

    return 0;
}
