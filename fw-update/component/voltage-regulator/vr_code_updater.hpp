#pragma once

#include "fw-update/common/fw_manager.hpp"

// code updater for voltage regulator
class VRCodeUpdater : FWManager
{
  public:
    VRCodeUpdater(sdbusplus::async::context& io, bool isDryRun);

    // override CodeUpdater functions

    bool verifyImage(sdbusplus::message::unix_fd image) override;
    void startUpdateAsync() override;

    // VR specific functions
    static void writeToVR(uint32_t bus, uint8_t addr, const uint8_t* fw,
                          size_t fw_size);
};
