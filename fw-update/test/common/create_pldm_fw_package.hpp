#pragma once

#include <inttypes.h>

#include <string>

std::string create_pldm_package(uint8_t* component_image,
                                size_t component_image_size);
