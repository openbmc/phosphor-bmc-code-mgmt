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

int verifyPLDMPackage(sdbusplus::message::unix_fd image)
{
    FILE* file = fdopen(image.fd, "r");

    if (!file)
    {
        perror("Failed to open PLDM firmware update package");
        return EXIT_FAILURE;
    }

    const size_t package_size = filesize(file);

    lg2::debug("allocate 512 bytes on the stack for the package header");

    std::array<uint8_t, 512> arr;

    int status = readPLDMPackage(file, arr.data(), package_size);

    fclose(file);

    if (status != 0)
    {
        lg2::error("failed to extract pldm package data");
        return EXIT_FAILURE;
    }

    struct pldm_package_header_information hinfo;
    struct variable_field vfptr;

    status = decode_pldm_package_header_info((const uint8_t*)arr.data(),
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

    // TODO: do the verification

    return EXIT_SUCCESS;
}
