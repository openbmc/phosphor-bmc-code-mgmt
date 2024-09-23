#pragma once

#include <unistd.h>

#include <cstdint>
#include <optional>
#include <string>

ssize_t create_pldm_firmware_device_identification_record(
    uint8_t* b, ssize_t i, const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible,
    uint16_t componentBitmapBitLength);

ssize_t create_pldm_firmware_device_identification_area_v1_0_0(
    uint8_t* b, ssize_t i, const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible,
    uint16_t componentBitmapBitLength);
