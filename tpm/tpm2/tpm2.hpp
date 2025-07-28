#pragma once

#include "tpm/tpm_device.hpp"

#include <string_view>

class TPM2Interface : public TPMInterface
{
  public:
    TPM2Interface(uint8_t tpmIndex) : TPMInterface(tpmIndex) {}

    bool isUpdateSupported() const final
    {
        // Currently, we do not support TPM2 firmware updates
        return false;
    }

    sdbusplus::async::task<bool> updateFirmware(const uint8_t* image,
                                                size_t image_size) final;

    sdbusplus::async::task<bool> getVersion(std::string& version) final;

  private:
    std::string getTPMResourceManagerPath() const
    {
        return "/dev/tpmrm" + std::to_string(tpmIndex);
    }

    sdbusplus::async::task<bool> getProperty(std::string_view property,
                                             uint32_t& value);
};
