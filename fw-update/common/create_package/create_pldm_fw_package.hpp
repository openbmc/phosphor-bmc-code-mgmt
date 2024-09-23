#pragma once

#include <inttypes.h>

#include <memory>
#include <string>

std::string create_pldm_package(uint8_t* component_image,
                                size_t component_image_size);

std::shared_ptr<uint8_t[]>
    create_pldm_package_buffer(const uint8_t* component_image,
                               size_t component_image_size, size_t* size_out);
