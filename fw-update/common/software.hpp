#pragma once

#include "fw_manager.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <string>

const auto actActive = sdbusplus::common::xyz::openbmc_project::software::
    Activation::Activations::Active;

// This represents a software version running on the device
class Software
{
  public:
    Software(sdbusplus::bus_t& bus, const char* objPath,
             const std::string& softwareId, const bool isDryRun) :
        swid(softwareId), dryRun(isDryRun), versionIntf(bus, objPath),
        activationIntf(bus, objPath), assocIntf(bus, objPath)
    {
        lg2::info(
            "Create functional association from Version to Inventory Item");

        std::string inventoryPath = "/xyz/openbmc_project/TODO";
        assocIntf.associations(
            {{objPath, inventoryPath, "how_do_i_create_association"}}, false);

        activationIntf.activation(actActive, false);
    };
    sdbusplus::message::object_path getObjectPath();

    std::string swid;
    bool dryRun;

    sdbusplus::server::object_t<
        sdbusplus::server::xyz::openbmc_project::software::Version>
        versionIntf;
    sdbusplus::server::object_t<
        sdbusplus::server::xyz::openbmc_project::software::Activation>
        activationIntf;
    sdbusplus::server::object_t<
        sdbusplus::server::xyz::openbmc_project::association::Definitions>
        assocIntf;
};
