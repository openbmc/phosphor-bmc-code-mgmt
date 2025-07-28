#pragma once

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"

#include <unordered_map>

namespace SoftwareInf = phosphor::software;
namespace ManagerInf = SoftwareInf::manager;

enum class TPMType
{
    TPM2,
};

class TPMInterface
{
  public:
    TPMInterface(uint8_t tpmIndex) : tpmIndex(tpmIndex) {}
    virtual ~TPMInterface() = default;
    TPMInterface(const TPMInterface&) = delete;
    TPMInterface& operator=(const TPMInterface&) = delete;
    TPMInterface(TPMInterface&&) = delete;
    TPMInterface& operator=(TPMInterface&&) = delete;

    virtual bool isUpdateSupported() const = 0;
    virtual sdbusplus::async::task<bool> getVersion(std::string& version) = 0;
    virtual sdbusplus::async::task<bool> updateFirmware(const uint8_t* image,
                                                        size_t imageSize) = 0;
    uint8_t tpmIndex;
};

class TPMDevice : public Device
{
  public:
    using Device::softwareCurrent;

    TPMDevice(sdbusplus::async::context& ctx, enum TPMType tpmType,
              uint8_t tpmIndex, SoftwareConfig& config,
              ManagerInf::SoftwareManager* parent);

    sdbusplus::async::task<bool> updateDevice(const uint8_t* image,
                                              size_t image_size) final;

    sdbusplus::async::task<std::string> getVersion();

    bool isUpdateSupported() const
    {
        return tpmInterface ? tpmInterface->isUpdateSupported() : false;
    }

  private:
    std::unique_ptr<TPMInterface> tpmInterface;
};

inline bool stringToTPMType(const std::string& type, TPMType& tpmType)
{
    static const std::unordered_map<std::string, TPMType> tpmTypeMap = {
        {"TPM2Firmware", TPMType::TPM2},
    };

    auto it = tpmTypeMap.find(type);
    if (it != tpmTypeMap.end())
    {
        tpmType = it->second;
        return true;
    }
    return false;
}
