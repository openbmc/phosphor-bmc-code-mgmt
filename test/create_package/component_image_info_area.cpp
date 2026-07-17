#include "component_image_info_area.hpp"

#include <inttypes.h>
#include <unistd.h>

// ComponentImageInformation (Table 5) for one image
static ssize_t create_pldm_component_image_info_v1_0_0(
    uint8_t* b, ssize_t i, uint16_t componentIdentifier,
    const TestComponentImage& component, size_t& componentLocationOffsetIndex)
{
    const size_t component_image_size = component.data.size();

    // ComponentClassification
    b[i++] = 0x1; // this is vendor selected value
    b[i++] = 0x0;

    // ComponentIdentifier
    b[i++] = (componentIdentifier >> 0) & 0xff;
    b[i++] = (componentIdentifier >> 8) & 0xff;

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
    componentLocationOffsetIndex = i;
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

    // ComponentVersionStringLength
    b[i++] = component.version.size();

    // ComponentVersionString
    for (const char v : component.version)
    {
        b[i++] = v;
    }

    return i;
}

/*
 * componentLocationOffsetIndices is for backfilling by the caller
 */
ssize_t create_pldm_component_image_info_area_v1_0_0(
    uint8_t* b, ssize_t i, std::span<const TestComponentImage> components,
    std::vector<size_t>& componentLocationOffsetIndices)
{
    // Component Image Count
    b[i++] = (components.size() >> 0) & 0xff;
    b[i++] = (components.size() >> 8) & 0xff;

    componentLocationOffsetIndices.resize(components.size());
    for (size_t c = 0; c < components.size(); c++)
    {
        // ComponentIdentifier is vendor selected, keep it unique per image
        i = create_pldm_component_image_info_v1_0_0(
            b, i, c + 1, components[c], componentLocationOffsetIndices[c]);
    }

    return i;
}
