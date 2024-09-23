#pragma once

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class MySwUpdate;

#include "fw-update/common/software.hpp"
#include "my_sw_update.hpp"
#include "spi_device.hpp"

class BiosSW : public Software
{
  public:
    BiosSW(sdbusplus::async::context& ctx, sdbusplus::bus_t& bus,
           const std::string& swid, const char* objPath, SPIDevice* parent);

    SPIDevice* parent;
    MySwUpdate updateIntf;
};
