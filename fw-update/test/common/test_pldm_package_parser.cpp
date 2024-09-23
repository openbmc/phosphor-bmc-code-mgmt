
#include "fw-update/common/create_package/create_pldm_fw_package.hpp"
#include "fw-update/common/pldm/package_parser.hpp"
#include "fw-update/common/pldm/types.hpp"
#include "fw-update/common/pldm_fw_update_verifier.hpp"
#include "phosphor-logging/lg2.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <tuple>

using namespace pldm::fw_update;

static void test_component_image(ComponentImageInfo& compImage,
                                 const std::shared_ptr<uint8_t[]>& buf,
                                 size_t component_image_size)
{
    const uint16_t compClass = std::get<0>(compImage);
    lg2::debug("component classification: {VALUE}", "VALUE", compClass);

    const uint16_t compIdentifier = std::get<1>(compImage);
    lg2::debug("component identifier: {VALUE}", "VALUE", compIdentifier);

    const uint32_t compLocationOffset = std::get<5>(compImage);
    lg2::debug("component location offset: {VALUE}", "VALUE",
               compLocationOffset);

    const uint32_t compSize = std::get<6>(compImage);
    lg2::debug("component size: {VALUE}", "VALUE", compSize);

    assert(compSize == component_image_size);

    std::string compVersion = std::get<7>(compImage);
    lg2::debug("component version: {VALUE}", "VALUE", compVersion);

    // print first few bytes of the component image
    uint8_t* p = buf.get() + compLocationOffset;
    lg2::debug("first few bytes of component image: {1} {2} {3} {4}", "1",
               lg2::hex, p[0], "2", lg2::hex, p[1], "3", lg2::hex, p[2], "4",
               lg2::hex, p[3]);

    assert(p[0] == 0xab);
    assert(p[1] == 0xba);
    assert(p[2] == 0xcd);
    assert(p[3] == 0xef);
}

static void test_firmware_device_id(FirmwareDeviceIDRecord& record)
{
    DeviceUpdateOptionFlags optFlags = std::get<0>(record);

    // assert we do not continue component updates after failure
    assert(!optFlags[0]);

    ApplicableComponents ac = std::get<1>(record);

    assert(ac.size() == 1);

    // assert that the one component image is applicable to this device
    assert(ac[0] == 0);

    ComponentImageSetVersion cisv = std::get<2>(record);

    assert(!cisv.empty());

    Descriptors desc = std::get<3>(record);

    assert(desc.size() == 1);

    assert(desc.contains(1)); // iana type descriptor

    auto v = desc[1];

    DescriptorData data = std::get<DescriptorData>(v);

    // iana id is 4 bytes
    assert(data.size() == 4);
}

int main()
{
    int status = 0;
    uint8_t component_image[] = {0xab, 0xba, 0xcd, 0xef};
    const size_t component_image_size = 4;

    std::string filename =
        create_pldm_package(component_image, component_image_size);

    if (filename.empty())
    {
        return 1;
    }

    FILE* file = fopen(filename.c_str(), "r");

    if (file == NULL)
    {
        lg2::error("could not create sample pldm fw update package");
        return 1;
    }

    const size_t size = filesize(file);

    auto buf = std::make_shared<uint8_t[]>(size);

    status = readPLDMPackage(file, buf.get(), size);

    if (status != 0)
    {
        return status;
    }

    fclose(file);

    std::shared_ptr<PackageParser> pp = parsePLDMFWUPPackageComplete(buf, size);

    if (pp == nullptr)
    {
        lg2::error("could not parse package header");
        return 1;
    }

    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    lg2::debug("found {N} fw device id records", "N", fwDeviceIdRecords.size());

    auto compImages = pp->getComponentImageInfos();
    lg2::debug("found {N} component images", "N", compImages.size());

    assert(compImages.size() == 1);

    ComponentImageInfo& compImage = compImages[0];

    test_component_image(compImage, buf, component_image_size);

    FirmwareDeviceIDRecords records = pp->getFwDeviceIDRecords();

    assert(records.size() == 1);

    test_firmware_device_id(records[0]);

    return status;
}
