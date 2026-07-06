#pragma once
#include "common/include/NotifyWatch.hpp"
#include "common/include/host_power.hpp"
#include "spi_device.hpp"

using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::software::host_power;

class BIOSDevice : public SPIDevice
{
  public:
    BIOSDevice(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
               uint64_t spiDeviceIndex, bool dryRun,
               const std::vector<std::string>& gpioLinesIn,
               const std::vector<bool>& gpioValuesIn, SoftwareConfig& config,
               SoftwareManager* parent);

    std::string getVersion() override;

    /** @brief Called by NotifyWatch when the version file is rewritten.
     *  @param inVersionFilename  name of the file that changed
     */
    sdbusplus::async::task<> processUpdate(std::string inVersionFilename);

  protected:
    sdbusplus::async::task<bool> preUpdate() override;
    sdbusplus::async::task<bool> postUpdate() override;

  private:
    phosphor::notify::watch::NotifyWatch<BIOSDevice> versionWatch;
    HostState prevPowerstate;
};
