#pragma once

#include <inttypes.h>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

constexpr const char* exampleVersion = "mycompversion";

struct TestComponentImage
{
    std::vector<uint8_t> data;
    std::string version;
    // Set in the ApplicableComponents bitmap of the device record
    bool applicable = true;
};

std::optional<std::string> create_pldm_package(uint8_t* component_image,
                                               size_t component_image_size);

std::unique_ptr<uint8_t[]> create_pldm_package_buffer(
    const uint8_t* component_image, size_t component_image_size,
    const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible, size_t& size_out);

// Package with one device record and up to 8 component images
std::unique_ptr<uint8_t[]> create_pldm_package_buffer(
    std::span<const TestComponentImage> components,
    const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible, size_t& size_out);
