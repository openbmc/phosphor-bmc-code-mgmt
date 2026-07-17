
#include "component_image_info_area.hpp"
#include "firmware_device_id_area.hpp"
#include "phosphor-logging/lg2.hpp"

#include <inttypes.h>
#include <libpldm/edac.h>
#include <libpldm/firmware_update.h>

#include <cstring>
#include <fstream>
#include <limits>
#include <random>

std::unique_ptr<uint8_t[]> create_pldm_package_buffer(
    std::span<const TestComponentImage> components,
    const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible, size_t& size_out)
{
    size_t size = 512;
    uint8_t applicableComponents = 0;
    for (size_t c = 0; c < components.size(); c++)
    {
        size += 64 + components[c].data.size();
        if (components[c].applicable)
        {
            applicableComponents |= 1 << c;
        }
    }
    auto buffer = std::make_unique<uint8_t[]>(size);
    uint8_t* b = buffer.get();
    memset(b, 0, size);
    ssize_t i = 0; // index

    // populate uuid
    // must be this value to align with
    // https://github.com/openbmc/pldm/blob/master/fw-update/package_parser.cpp#L294
    uint8_t uuid[PLDM_FWUP_UUID_LENGTH] = {
        0xF0, 0x18, 0x87, 0x8C, 0xCB, 0x7D, 0x49, 0x43,
        0x98, 0x00, 0xA0, 0x2F, 0x05, 0x9A, 0xCA, 0x02};
    memcpy(b, uuid, PLDM_FWUP_UUID_LENGTH);

    i += PLDM_FWUP_UUID_LENGTH;

    // package header format revision
    b[i++] = 0x01;
    // must be 1 to align with
    // https://github.com/openbmc/pldm/blob/master/fw-update/package_parser.cpp#L294

    // package header size (leave space)
    ssize_t package_header_size_offset = i;
    i += 2;

    // package release date time
    // set timestamp as unknown value
    b[i + 12] = 15;
    i += PLDM_TIMESTAMP104_SIZE;

    // component bitmap bit length
    const uint16_t componentBitmapBitLength = 8;
    b[i++] = componentBitmapBitLength;
    b[i++] = 0x00;

    // package_version_string_type
    b[i++] = 0x01; // type = ASCII

    const char* package_version_str = (const char*)"VersionString1";
    // package version string length
    b[i++] = strlen(package_version_str);

    // package version string
    for (size_t j = 0; j < strlen(package_version_str); j++)
    {
        b[i++] = package_version_str[j];
    }

    // --- Firmware Device Identification Area 1.0.0 ---

    i = create_pldm_firmware_device_identification_area_v1_0_0(
        b, i, optVendorIANA, optCompatible, componentBitmapBitLength,
        applicableComponents);

    // --- Component Image Information Area 1.0.0 ---
    std::vector<size_t> componentLocationOffsetIndices;
    i = create_pldm_component_image_info_area_v1_0_0(
        b, i, components, componentLocationOffsetIndices);

    // PackageHeaderChecksum (backfill later)
    const size_t packageHeaderChecksumOffset = i;
    i += 4;

    // backfill the PackageHeaderSize
    b[package_header_size_offset + 0] = (i >> 0) & 0xff;
    b[package_header_size_offset + 1] = (i >> 8) & 0xff;

    // backfill the ComponentLocationOffsets, images follow the header in order
    size_t location = i;
    for (size_t c = 0; c < components.size(); c++)
    {
        const size_t index = componentLocationOffsetIndices[c];
        b[index + 0] = (location >> 0) & 0xff;
        b[index + 1] = (location >> 8) & 0xff;
        b[index + 2] = (location >> 16) & 0xff;
        b[index + 3] = (location >> 24) & 0xff;
        location += components[c].data.size();
    }

    // backfill PackageHeaderChecksum
    const uint32_t crc = pldm_edac_crc32(b, packageHeaderChecksumOffset);
    memcpy(b + packageHeaderChecksumOffset, &crc, 4);

    // --- end of the package header ---

    // write the component images
    for (const TestComponentImage& component : components)
    {
        for (const uint8_t v : component.data)
        {
            b[i++] = v;
        }
    }

    lg2::debug("wrote {NBYTES} bytes for pldm update package", "NBYTES", i);

    size_out = i;
    return buffer;
}

std::unique_ptr<uint8_t[]> create_pldm_package_buffer(
    const uint8_t* component_image, size_t component_image_size,
    const std::optional<uint32_t>& optVendorIANA,
    const std::optional<std::string>& optCompatible, size_t& size_out)
{
    const std::vector<TestComponentImage> components = {
        {std::vector<uint8_t>(component_image,
                              component_image + component_image_size),
         exampleVersion}};
    return create_pldm_package_buffer(components, optVendorIANA, optCompatible,
                                      size_out);
}

static void create_pldm_package_file(std::ofstream& of,
                                     const uint8_t* component_image,
                                     size_t component_image_size)
{
    size_t size;
    std::unique_ptr<uint8_t[]> buf =
        create_pldm_package_buffer(component_image, component_image_size,
                                   std::nullopt, std::nullopt, size);
    uint8_t* b = buf.get();

    of.write(reinterpret_cast<char*>(b), (long)size);
}

std::optional<std::string> create_pldm_package(uint8_t* component_image,
                                               size_t component_image_size)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, std::numeric_limits<int>::max());

    std::string filename =
        std::format("/tmp/pldm-package-{}.bin", distrib(gen));

    std::ofstream of(filename, std::ofstream::out);

    if (!of.good())
    {
        lg2::error("could not create file: {FILENAME}", "FILENAME", filename);
        return std::nullopt;
    }

    create_pldm_package_file(of, component_image, component_image_size);

    of.close();

    return filename;
}
