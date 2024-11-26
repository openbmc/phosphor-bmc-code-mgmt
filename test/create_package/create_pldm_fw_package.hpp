#pragma once

#include <inttypes.h>

#include <memory>
#include <optional>
#include <string>

std::optional<std::string>
    create_pldm_package(uint8_t* component_image, size_t component_image_size);

std::unique_ptr<uint8_t[]> create_pldm_package_buffer(
    const uint8_t* component_image, size_t component_image_size,
    const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible, size_t& size_out);
