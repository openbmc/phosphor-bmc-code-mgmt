#pragma once

#include "create_pldm_fw_package.hpp"

#include <unistd.h>

#include <cstdint>
#include <span>
#include <vector>

/*
 * componentLocationOffsetIndices is for backfilling by the caller
 */
ssize_t create_pldm_component_image_info_area_v1_0_0(
    uint8_t* b, ssize_t i, std::span<const TestComponentImage> components,
    std::vector<size_t>& componentLocationOffsetIndices);
