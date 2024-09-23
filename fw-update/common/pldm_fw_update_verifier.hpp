#pragma once

#include "fw-update/common/pldm/package_parser.hpp"
#include "fw-update/common/pldm/types.hpp"
#include "sdbusplus/message/native_types.hpp"

#include <cstdint>
#include <memory>

using namespace pldm::fw_update;

size_t filesize(FILE* file);

std::shared_ptr<PackageParser> parsePLDMFWUPPackageComplete(
    const std::shared_ptr<uint8_t[]>& buf, size_t size);

int readPLDMPackage(FILE* file, uint8_t* package_data, size_t package_size);

int verifyPLDMPackage(sdbusplus::message::unix_fd image);
