#pragma once

#include "tpm/tpm_device.hpp"

#include <string_view>

class TPM2Interface : public TPMInterface
{
  public:
    TPM2Interface() = default;

    bool isUpdateSupported() const final
    {
        // Currently, we do not support TPM2 firmware updates
        return false;
    }

    sdbusplus::async::task<bool> updateFirmware(const uint8_t* image,
                                                size_t image_size) final;

    sdbusplus::async::task<bool> getVersion(std::string& version) final;

  private:
    static sdbusplus::async::task<bool> getValue(std::string_view key,
                                                 uint32_t& value);
};
