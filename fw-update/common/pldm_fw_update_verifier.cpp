#include <fcntl.h>
#include <libpldm/base.h>
#include <libpldm/firmware_update.h>
#include <libpldm/pldm_types.h>
#include <libpldm/utils.h>
#include <stdio.h>
#include <stdlib.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

// TODO: find a correct size for this.
#define PLDM_PACKAGE_MAX_SIZE 10000

static size_t filesize(FILE* file)
{
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

static uint8_t* readPLDMPackage(FILE* file,
                                std::array<uint8_t, PLDM_PACKAGE_MAX_SIZE> arr,
                                const size_t package_size)
{
    // Allocate memory to read the package
    uint8_t* package_data = arr.data();
    if (!package_data)
    {
        perror("Failed to allocate memory for package data");
        fclose(file);
        return NULL;
    }

    // Read the package into memory
    if (fread(package_data, 1, package_size, file) != package_size)
    {
        perror("Failed to read package data");
        fclose(file);
        return NULL;
    }

    return package_data;
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

    std::array<uint8_t, PLDM_PACKAGE_MAX_SIZE> arr;

    uint8_t* package_data = readPLDMPackage(file, arr, package_size);

    fclose(file);

    if (package_data == NULL)
    {
        return EXIT_FAILURE;
    }

    struct pldm_package_header_information hinfo;
    struct variable_field* vfptr = NULL;

    int status = decode_pldm_package_header_info((const uint8_t*)package_data,
                                                 package_size, &hinfo, vfptr);

    if (status != PLDM_SUCCESS)
    {
        lg2::error("could not decode pldm package header info");
        return EXIT_FAILURE;
    }

    lg2::debug("decoded pldm package header info:");
    lg2::debug("package header format version: {VERSION}", "VERSION",
               hinfo.package_header_format_version);
    lg2::debug("package header size: {SIZE} bytes", "SIZE",
               (int)hinfo.package_header_size);
    lg2::debug("package version string: {VERSION_STR}", "VERSION_STR",
               vfptr->ptr);

    // TODO: do the verification

    return EXIT_SUCCESS;
}
