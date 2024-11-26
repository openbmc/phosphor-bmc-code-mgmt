
#include "fw-update/common/pldm/package_parser.hpp"
#include "fw-update/common/pldm/pldm_util.hpp"
#include "fw-update/common/pldm/types.hpp"
#include "fw-update/tool/create_package/create_pldm_fw_package.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

using namespace pldm::fw_update;

int main()
{
    int status = 0;
    uint8_t component_image[] = {0xab, 0xba, 0xcd, 0xef};
    const size_t component_image_size = 4;

    const std::string filename =
        create_pldm_package(component_image, component_image_size);

    assert(!filename.empty());

    FILE* file = fopen(filename.c_str(), "r");

    assert(file != NULL);

    const size_t size = filesize(file);

    auto buf = std::make_shared<uint8_t[]>(size);

    status = pldmutil::readPLDMPackage(file, buf.get(), size);

    fclose(file);

    if (status != 0)
    {
        return status;
    }

    std::shared_ptr<PackageParser> pp =
        pldmutil::parsePLDMFWUPPackageComplete(buf.get(), size);

    assert(pp != nullptr);

    auto fwDeviceIdRecords = pp->getFwDeviceIDRecords();
    lg2::debug("found {N} fw device id records", "N", fwDeviceIdRecords.size());

    const FirmwareDeviceIDRecords records = pp->getFwDeviceIDRecords();

    assert(records.size() == 1);

    const FirmwareDeviceIDRecord& record = records[0];

    const DeviceUpdateOptionFlags optFlags = std::get<0>(record);

    // assert we do not continue component updates after failure
    assert(!optFlags[0]);

    const ApplicableComponents ac = std::get<1>(record);

    assert(ac.size() == 1);

    // assert that the one component image is applicable to this device
    assert(ac[0] == 0);

    const ComponentImageSetVersion cisv = std::get<2>(record);

    assert(!cisv.empty());

    Descriptors desc = std::get<3>(record);

    assert(desc.size() == 1);

    assert(desc.contains(1)); // iana type descriptor

    auto v = desc[1];

    const DescriptorData data = std::get<DescriptorData>(v);

    // iana id is 4 bytes
    assert(data.size() == 4);

    return status;
}
