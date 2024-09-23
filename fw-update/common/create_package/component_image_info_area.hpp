#pragma once

#include <unistd.h>

#include <cstdint>

/*
 * componentLocationOffsetIndex  is for backfilling by the caller
 */
ssize_t create_pldm_component_image_info_area_v1_0_0(
    uint8_t* b, ssize_t i, size_t component_image_size,
    size_t* componentLocationOffsetIndex);
