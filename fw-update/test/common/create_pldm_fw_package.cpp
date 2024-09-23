
#include "fw-update/common/pldm_fw_update_verifier.hpp"
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
#include <fstream>
#include <limits>
#include <random>

ssize_t create_pldm_component_image_info_area(uint8_t* b, ssize_t i)
{
    // Component Image Count
    b[i++] = 0x1;
    b[i++] = 0x0;

    // ComponentImageInformation (Table 6)
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
    size_t locationOffset = i;
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;

    // ComponentSize
    b[i++] = 0x2;
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;

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

    // ComponentOpaqueDataLength
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;
    b[i++] = 0x0;

    // backfill the component location offset
    b[locationOffset] = i;

    // ComponentOpaqueData
    // TODO: use real data
    b[i++] = 0xab;
    b[i++] = 0xab;

    return i;
}

int create_pldm_package2(std::ofstream& of)
{
    uint8_t b[4096]; // buffer
    memset(b, 0, 4096);
    ssize_t i = 0;   // index

    // populate uuid
    uint8_t uuid[PLDM_FWUP_UUID_LENGTH] = {
        0xF0, 0x18, 0x87, 0x8C, 0xCB, 0x7D, 0x49, 0x43,
        0x98, 0x00, 0xA0, 0x2F, 0x05, 0x9A, 0xCA, 0x02};
    memcpy(b, uuid, PLDM_FWUP_UUID_LENGTH);

    i += PLDM_FWUP_UUID_LENGTH;

    // package header format revision
    b[i++] = 0x04;

    // package header size (leave space)
    ssize_t package_header_size_offset = i;
    i += 2;

    // package release date time
    // set timestamp as unknown value
    b[i + 12] = 15;
    i += PLDM_TIMESTAMP104_SIZE;

    // component bitmap bit length
    b[i++] = 0x08;
    b[i++] = 0x00;

    // package_version_string_type
    b[i++] = 0x01; // type = ASCII

    // as in the example
    const char* package_version_str = (const char*)"VersionString1";
    // package version string length
    b[i++] = strlen(package_version_str);

    // package version string
    for (size_t j = 0; j < strlen(package_version_str); j++)
    {
        b[i++] = package_version_str[j];
    }

    // --- Firmware Device Identification Area ---

    // Device ID Record Count
    b[i++] = 0;

    // --- Downstream Device Identification Area ---

    // Downstream Device ID Record Count
    b[i++] = 0;

    // --- Component Image Information Area ---
    i = create_pldm_component_image_info_area(b, i);

    // PackageHeaderChecksum
    uint32_t crc = crc32(&b, i);
    memcpy(b + i, &crc, 4);
    i += 4;

    // PLDMFWPackagePayloadChecksum
    // we have no payload for now
    b[i++] = 1;
    b[i++] = 0;
    b[i++] = 0;
    b[i++] = 0;

    b[package_header_size_offset + 0] = (i >> 0) & 0xff;
    b[package_header_size_offset + 1] = (i >> 8) & 0xff;

    lg2::debug("wrote {NBYTES} bytes for pldm update package", "NBYTES", i);

    of.write(reinterpret_cast<char*>(b), i);

    return 0;
}

std::string create_pldm_package()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, std::numeric_limits<int>::max());

    std::string filename =
        std::format("/tmp/pldm-package-{}.bin", distrib(gen));

    std::ofstream of(filename, std::ofstream::out);

    if (!of.good())
    {
        return "";
    }

    create_pldm_package2(of);

    of.close();

    return filename;
}
