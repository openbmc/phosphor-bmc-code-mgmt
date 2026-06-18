#include "common/include/NotifyWatch.hpp"
#include "common/include/spi/spi_device.hpp"

class SPIBIOS;

using namespace phosphor::notify::watch;

using NotifyWatchIntf = phosphor::notify::watch::NotifyWatch<SPIBIOS>;

const std::string biosVersionDirPath = "/var/bios/";
const std::string biosVersionFilename = "host0_bios_version.txt";
const std::string biosVersionPath = biosVersionDirPath + biosVersionFilename;

const std::string versionUnknown = "Unknown";

class SPIBIOS : public SPIDevice, public NotifyWatchIntf
{
  public:
    SPIBIOS(sdbusplus::async::context& ctx, uint64_t spiControllerIndex,
            uint64_t spiDeviceIndex, bool dryRun,
            const std::vector<std::string>& gpioLinesIn,
            const std::vector<bool>& gpioValuesIn, SoftwareConfig& config,
            SoftwareManager* parent, enum FlashLayout layout,
            enum FlashTool tool,
            const std::string& versionDirPath = biosVersionDirPath);

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    /** @brief Process async changes to cable configuration */
    auto processUpdate(std::string versionFileName) -> sdbusplus::async::task<>;

  protected:
    std::string getVersion() const final;
};
