#include "bios_sw.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstdlib>

BiosSW::BiosSW(sdbusplus::bus_t& bus, const std::string& swid,
               const char* objPath, SPIDeviceCodeUpdater* parent) :
    Software(bus, objPath, swid, true), parent(parent),
    updateIntf(bus, objPath, this)
{}
