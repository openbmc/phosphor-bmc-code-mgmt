
#include "common/pldm/package_parser.hpp"
#include "common/pldm/pldm_util.hpp"
#include "common/pldm/types.hpp"
#include "tool/create_package/create_pldm_fw_package.hpp"

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

    std::string filename =
        create_pldm_package(component_image, component_image_size);

    assert(!filename.empty());

    FILE* file = fopen(filename.c_str(), "r");

    assert(file != NULL);

    const size_t size = filesize(file);

    auto buf = std::make_shared<uint8_t[]>(size);

    status = pldmutil::readPLDMPackage(file, buf.get(), size);

    fclose(file);

    assert(status == 0);

    std::shared_ptr<PackageParser> pp =
        pldmutil::parsePLDMFWUPPackageComplete(buf.get(), size);

    assert(pp != nullptr);

    auto compImages = pp->getComponentImageInfos();

    lg2::debug("found {N} component images", "N", compImages.size());

    assert(compImages.size() == 1);

    const ComponentImageInfo& compImage = compImages[0];

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

    uint8_t* p = buf.get() + compLocationOffset;
    lg2::debug("first few bytes of component image: {1} {2} {3} {4}", "1",
               lg2::hex, p[0], "2", lg2::hex, p[1], "3", lg2::hex, p[2], "4",
               lg2::hex, p[3]);

    assert(p[0] == 0xab);
    assert(p[1] == 0xba);
    assert(p[2] == 0xcd);
    assert(p[3] == 0xef);

    return status;
}
