
#include "phosphor-logging/lg2.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <libpldm/base.h>
#include <libpldm/firmware_update.h>
#include <libpldm/pldm_types.h>
#include <libpldm/utils.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

/*
 * componentLocationOffsetIndex  is for backfilling by the caller
 */
ssize_t create_pldm_component_image_info_area_v1_0_0(
    uint8_t* b, ssize_t i, size_t component_image_size,
    size_t* componentLocationOffsetIndex)
{
    // Component Image Count
    b[i++] = 0x1;
    b[i++] = 0x0;

    // ComponentImageInformation (Table 5)
    // (1 for each image)

    // ComponentClassification
    b[i++] = 0x1; // this is vendor selected value
    b[i++] = 0x0;

    // ComponentIdentifier
    b[i++] = 0x1; // this is vendor selected value
    b[i++] = 0x0;

    // ComponentComparisonStamp
    b[i++] = 0xff;
    b[i++] = 0xff;
    b[i++] = 0xff;
    b[i++] = 0xff;

    // ComponentOptions
    b[i++] = 0x00;
    b[i++] = 0x00;

    // RequestedComponentActivationMethod
    b[i++] = 0b100000; // AC Power Cycle
    b[i++] = 0x0;

    // ComponentLocationOffset
    // (leave blank for now)
    *componentLocationOffsetIndex = i;
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;

    // ComponentSize
    b[i++] = (component_image_size >> 0) & 0xff;
    b[i++] = (component_image_size >> 8) & 0xff;
    b[i++] = (component_image_size >> 16) & 0xff;
    b[i++] = (component_image_size >> 24) & 0xff;

    // ComponentVersionStringType
    b[i++] = 0x1; // type = Ascii

    const char* buf = (const char*)"mycompversion";
    // ComponentVersionStringLength
    b[i++] = strlen(buf);

    // ComponentVersionString
    for (ssize_t j = 0; j < (ssize_t)strlen(buf); j++)
    {
        b[i++] = buf[j];
    }

    return i;
}
