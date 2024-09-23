#include <fcntl.h>
#include <libpldm/base.h>
#include <libpldm/firmware_update.h>
#include <libpldm/pldm_types.h>
#include <libpldm/utils.h>
#include <stdio.h>
#include <stdlib.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cassert>
#include <cstring>

static size_t filesize(FILE* file)
{
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

static int readPLDMPackage(FILE* file, uint8_t* package_data,
                           const size_t package_size)
{
    assert(file != NULL);
    assert(package_data != NULL);

    lg2::debug("reading {NBYTES} bytes from file", "NBYTES", package_size);

    // Read the package into memory
    if (fread(package_data, 1, package_size, file) != package_size)
    {
        perror("Failed to read package data");
        fclose(file);
        return 1;
    }

    return 0;
}

int verifyPLDMComponentImage(uint8_t* data, size_t length)
{
    struct pldm_component_image_information ci;

    struct variable_field vf;
    int status = decode_pldm_comp_image_info(data, length, &ci, &vf);

    if (status != 0)
    {
        lg2::error("could not decode comp image data: {ERR}", "ERR", status);
        return status;
    }

    lg2::debug("component classification: {DATA}", "DATA",
               (int)ci.comp_classification);

    return 0;
}

int verifyPLDMPackage(sdbusplus::message::unix_fd image)
{
    FILE* file = fdopen(image.fd, "r");

    if (!file)
    {
        perror("Failed to open PLDM firmware update package");
        return EXIT_FAILURE;
    }

    const size_t package_size = filesize(file);

    const size_t total_length = 512;

    lg2::debug("allocate {NBYTES} bytes on the stack for the package header",
               "NBYTES", total_length);

    std::array<uint8_t, total_length> arr;
    uint8_t* data = arr.data();
    uint8_t* current_data = data;

    int status = readPLDMPackage(file, data, package_size);

    fclose(file);

    if (status != 0)
    {
        lg2::error("failed to extract pldm package data");
        return EXIT_FAILURE;
    }

    struct pldm_package_header_information hinfo;
    struct variable_field vfptr;

    status = decode_pldm_package_header_info((const uint8_t*)current_data,
                                             package_size, &hinfo, &vfptr);

    if (status != PLDM_SUCCESS)
    {
        lg2::error(
            "could not decode pldm package header info. completion code: {CCODE}",
            "CCODE", status);
        return status;
    }

    lg2::debug("decoded pldm package header info:");
    lg2::debug("package header format version: {VERSION}", "VERSION",
               hinfo.package_header_format_version);
    lg2::debug("package header size: {SIZE} bytes", "SIZE",
               (int)hinfo.package_header_size);

    char buf[256];
    memset(buf, 0, 256);
    memcpy(buf, vfptr.ptr, vfptr.length);
    lg2::debug("package version string: {VERSION_STR}", "VERSION_STR", buf);

    current_data = data + sizeof(struct pldm_package_header_information) +
                   vfptr.length;

    const uint8_t deviceIdRecordCount = current_data[0];

    if (deviceIdRecordCount > 0)
    {
        lg2::debug("found {N} device id records", "N", deviceIdRecordCount);
        // TODO
        return 1;
    }

    current_data += 1;

    const uint8_t downstreamDeviceIdRecordCount = *current_data;

    if (downstreamDeviceIdRecordCount > 0)
    {
        lg2::debug("found > 0 downstream device id records");
        // TODO
        return 1;
    }

    current_data += 1;

    uint16_t componentImageCount =
        (uint16_t)(current_data[0] | (current_data[1] << 8));
    current_data += 2;

    lg2::debug("found {N} component images", "N", componentImageCount);

    for (int i = 0; i < componentImageCount; i++)
    {
        // TODO: change offset

        const size_t offset = current_data - data;
        const size_t length = total_length - offset;
        if (verifyPLDMComponentImage(current_data, length) != 0)
        {
            return 1;
        }
    }

    // TODO: do the verification

    return EXIT_SUCCESS;
}
