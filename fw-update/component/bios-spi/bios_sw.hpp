#pragma once

// #include "phosphor-logging/lg2.hpp"
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class MySwUpdate;

#include "fw-update/common/software.hpp"
#include "my_sw_update.hpp"
#include "spi_device_code_updater.hpp"

class BiosSW : public Software
{
  public:
    BiosSW(const std::shared_ptr<sdbusplus::asio::connection>& conn,
           const std::string& swid, const char* objPath,
           SPIDeviceCodeUpdater* parent);

    SPIDeviceCodeUpdater* parent;
    MySwUpdate updateIntf;
};
