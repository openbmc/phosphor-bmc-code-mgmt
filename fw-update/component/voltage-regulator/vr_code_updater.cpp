#include "vr_code_updater.hpp"

#include "vr.hpp"

#include <phosphor-logging/lg2.hpp>

// reference code for VR fw update:
// https://github.com/facebook/openbmc/blob/helium/meta-facebook/meta-fbtp/recipes-fbtp/plat-libs/files/vr/vr.c#L1787

VRCodeUpdater::VRCodeUpdater(sdbusplus::async::context& io, bool isDryRun) :
    FWManager(io, "VR", isDryRun)
{
    lg2::info("VR Code Updater started");
    // TODO
}

std::string VRCodeUpdater::getConfigurationInterface()
{
    return "xyz.openbmc_project.Configuration.VRDevice";
}

bool VRCodeUpdater::verifyImage(sdbusplus::message::unix_fd image)
{
    lg2::info("VR Code Updater: verify image");
    (void)image;
    // TODO
    return true;
}

void VRCodeUpdater::writeToVR(uint32_t bus, uint8_t addr, const uint8_t* fw,
                              size_t fw_size)
{
    (void)bus;
    (void)addr;
    (void)fw;
    (void)fw_size;
    // TODO
    lg2::info("VR Code Updater: write to VR");

    // TODO
    uint8_t fru = 0;
    // TODO
    uint8_t board_info = 0;

    // TODO
    const char* file = "/tmp/vr_fw.bin";

    vr_fw_update(fru, board_info, file);

    lg2::info("\nUpdate VR Success!\n");
}
