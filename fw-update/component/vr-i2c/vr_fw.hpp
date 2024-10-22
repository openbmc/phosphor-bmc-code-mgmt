#pragma once

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

class VRFWUpdate;

#include "fw-update/common/software.hpp"
#include "i2c_vr_device.hpp"
#include "vr_fw_update.hpp"

class VRFW : public Software
{
  public:
    VRFW(sdbusplus::async::context& io, sdbusplus::bus_t& bus,
         const std::string& swid, const char* objPath, I2CVRDevice* parent);

    I2CVRDevice* parent;
    VRFWUpdate updateIntf;
};
