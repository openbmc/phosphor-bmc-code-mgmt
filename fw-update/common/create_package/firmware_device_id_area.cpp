
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
#include <vector>

ssize_t create_pldm_firmware_device_descriptor_v1_0_0(
    uint8_t* b, ssize_t i, uint16_t descType, std::vector<uint8_t>& data)
{
    // DescriptorType, Table 7. (0x0001 == IANA Enterprise ID, 4 bytes)
    b[i++] = (descType >> 0) & 0xff;
    b[i++] = (descType >> 8) & 0xff;

    // DescriptorLength
    b[i++] = (data.size() >> 0) & 0xff;
    b[i++] = (data.size() >> 8) & 0xff;

    // DescriptorData
    for (uint8_t v : data)
    {
        b[i++] = v;
    }

    return i;
}

ssize_t create_pldm_firmware_device_vendor_defined_descriptor_v1_0_0(
    uint8_t* b, ssize_t i, std::vector<uint8_t>& data)
{
    const uint16_t descType = 0xffff; // vendor defined

    // DescriptorType, Table 7.
    b[i++] = (descType >> 0) & 0xff;
    b[i++] = (descType >> 8) & 0xff;

    // DescriptorLength
    const uint16_t length = data.size() + 3; // data and the additional fields
    b[i++] = (length >> 0) & 0xff;
    b[i++] = (length >> 8) & 0xff;

    // VendorDefinedDescriptorTitleStringType
    b[i++] = 0x1; // type 1 = ascii

    // VendorDefinedDescriptorTitleStringLength
    b[i++] = data.size();

    // VendorDefinedDescriptorTitleString
    for (uint8_t v : data)
    {
        b[i++] = v;
    }

    // VendorDefinedDescriptorData
    b[i++] = 0x00;

    return i;
}

ssize_t create_pldm_firmware_device_descriptors_v1_0_0(uint8_t* b, ssize_t i,
                                                       size_t* actual_count)
{
    // RecordDescriptiors. The initial descriptor is restricted.
    // In our case we use iana for the first descriptor.

    std::vector<uint8_t> descriptorData = {0xaf, 0xaf, 0xaf, 0xaf};
    i = create_pldm_firmware_device_descriptor_v1_0_0(b, i, 0x1,
                                                      descriptorData);

    std::string comp = "com.example.vendor.device";
    std::vector<uint8_t> compatible = {};
    for (char c : comp)
    {
        compatible.push_back((uint8_t)c);
    }
    i = create_pldm_firmware_device_vendor_defined_descriptor_v1_0_0(
        b, i, compatible);

    *actual_count = 2;

    return i;
}

ssize_t create_pldm_firmware_device_identification_record(
    uint8_t* b, ssize_t i, uint16_t componentBitmapBitLength)
{
    const ssize_t startIndex = i;
    // RecordLength, backfill later
    const size_t recordLengthOffset = i;
    b[i++] = 0;
    b[i++] = 0;

    // DescriptorCount (backfill later)
    const size_t descriptorCountOffset = i;
    b[i++] = 0;

    // DeviceUpdateOptionFlags
    b[i++] = 0;
    b[i++] = 0;
    b[i++] = 0;
    b[i++] = 0;

    // ComponentImageSetVersionStringType
    b[i++] = 1; // type = Ascii

    const char* str = (const char*)"compimagesetversion1";
    // ComponentImageSetVersionStringLength
    b[i++] = strlen(str);

    // FirmwareDevicePackageDataLength
    b[i++] = 0x0;
    b[i++] = 0x0;

    // ApplicableComponents
    for (int j = 0; j < (componentBitmapBitLength / 8); j++)
    {
        b[i++] = 0x1;
        // the first and only component image does apply to this device
    }

    // ComponentSetVersionString
    for (size_t j = 0; j < strlen(str); j++)
    {
        b[i++] = str[j];
    }

    // RecordDescriptiors. The initial descriptor is restricted.
    size_t actual_count = 0;
    i = create_pldm_firmware_device_descriptors_v1_0_0(b, i, &actual_count);

    // backfill DescriptorCount
    b[descriptorCountOffset] = actual_count;

    // FirmwareDevicePackageData (optional, we make it empty)

    // backfill RecordLength
    const ssize_t recordLength = i - startIndex;
    b[recordLengthOffset + 0] = (recordLength >> 0) & 0xff;
    b[recordLengthOffset + 1] = (recordLength >> 8) & 0xff;

    return i;
}

ssize_t create_pldm_firmware_device_identification_area_v1_0_0(
    uint8_t* b, ssize_t i, uint16_t componentBitmapBitLength)
{
    // Device ID Record Count
    b[i++] = 1;

    return create_pldm_firmware_device_identification_record(
        b, i, componentBitmapBitLength);
}
