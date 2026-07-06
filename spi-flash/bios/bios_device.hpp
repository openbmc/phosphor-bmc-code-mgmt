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

  protected:
    sdbusplus::async::task<bool> preUpdate() override;
    sdbusplus::async::task<bool> postUpdate() override;

  private:
    /** @brief Re-read the version file and publish it on D-Bus. */
    void refreshVersion();

    /** @brief Watches the BIOS version directory and notifies the owning
     *         BIOSDevice when the version file is rewritten.
     */
    class VersionWatch :
        public phosphor::notify::watch::NotifyWatch<VersionWatch>
    {
      public:
        VersionWatch(sdbusplus::async::context& ctx, const std::string& dir,
                     BIOSDevice& owner) : NotifyWatch(ctx, dir), owner(owner)
        {}

        /** @brief Called by NotifyWatch when a watched file is written.
         *  @param inVersionFilename  name of the file that changed
         */
        sdbusplus::async::task<> processUpdate(std::string inVersionFilename);

      private:
        BIOSDevice& owner;
    };

    VersionWatch versionWatch;
    HostState prevPowerstate;
};
