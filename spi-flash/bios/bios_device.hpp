#pragma once
#include "common/include/NotifyWatch.hpp"
#include "common/include/host_power.hpp"
#include "spi_device.hpp"

class BIOSDevice;

using namespace phosphor::software;
using namespace phosphor::software::manager;
using namespace phosphor::software::host_power;

using NotifyWatchIntf = phosphor::notify::watch::NotifyWatch<BIOSDevice>;

class BIOSDevice : public SPIDevice, public NotifyWatchIntf
{
  public:
    BIOSDevice(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
               uint64_t spiDeviceIndex, bool dryRun,
               const std::vector<std::string>& gpioLinesIn,
               const std::vector<bool>& gpioValuesIn, SoftwareConfig& config,
               SoftwareManager* parent);

    std::string getVersion() override;

    /** @brief Process async changes to cable configuration */
    sdbusplus::async::task<> processUpdate(std::string versionFileName);

  protected:
    sdbusplus::async::task<bool> preUpdate() override;
    sdbusplus::async::task<bool> postUpdate() override;

  private:
    std::string versionDirPath;
    std::string versionFilename;
    HostState prevPowerstate;
};
