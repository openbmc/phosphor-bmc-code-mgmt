#pragma once

#include <sdbusplus/message/types.hpp>

#include <bitset>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// NOLINTBEGIN

namespace pldm
{

using Availability = bool;
using eid = uint8_t;
using UUID = std::string;

namespace fw_update
{

// Descriptor definition
using DescriptorType = uint16_t;
using DescriptorData = std::vector<uint8_t>;
using VendorDefinedDescriptorTitle = std::string;
using VendorDefinedDescriptorData = std::vector<uint8_t>;
using VendorDefinedDescriptorInfo =
    std::tuple<VendorDefinedDescriptorTitle, VendorDefinedDescriptorData>;
using Descriptors =
    std::map<DescriptorType,
             std::variant<DescriptorData, VendorDefinedDescriptorInfo>>;

using DescriptorMap = std::unordered_map<eid, Descriptors>;

// Component information
using CompClassification = uint16_t;
using CompIdentifier = uint16_t;
using CompKey = std::pair<CompClassification, CompIdentifier>;
using CompClassificationIndex = uint8_t;
using ComponentInfo = std::map<CompKey, CompClassificationIndex>;
using ComponentInfoMap = std::unordered_map<eid, ComponentInfo>;

// PackageHeaderInformation
using PackageHeaderSize = size_t;
using PackageVersion = std::string;
using ComponentBitmapBitLength = uint16_t;
using PackageHeaderChecksum = uint32_t;

// FirmwareDeviceIDRecords
using DeviceIDRecordCount = uint8_t;
using DeviceUpdateOptionFlags = std::bitset<32>;
using ApplicableComponents = std::vector<size_t>;
using ComponentImageSetVersion = std::string;
using FirmwareDevicePackageData = std::vector<uint8_t>;
using FirmwareDeviceIDRecord =
    std::tuple<DeviceUpdateOptionFlags, ApplicableComponents,
               ComponentImageSetVersion, Descriptors,
               FirmwareDevicePackageData>;
using FirmwareDeviceIDRecords = std::vector<FirmwareDeviceIDRecord>;

// ComponentImageInformation
using ComponentImageCount = uint16_t;
using CompComparisonStamp = uint32_t;
using CompOptions = std::bitset<16>;
using ReqCompActivationMethod = std::bitset<16>;
using CompLocationOffset = uint32_t;
using CompSize = uint32_t;
using CompVersion = std::string;
using ComponentImageInfo =
    std::tuple<CompClassification, CompIdentifier, CompComparisonStamp,
               CompOptions, ReqCompActivationMethod, CompLocationOffset,
               CompSize, CompVersion>;
using ComponentImageInfos = std::vector<ComponentImageInfo>;

enum class ComponentImageInfoPos : size_t
{
    CompClassificationPos = 0,
    CompIdentifierPos = 1,
    CompComparisonStampPos = 2,
    CompOptionsPos = 3,
    ReqCompActivationMethodPos = 4,
    CompLocationOffsetPos = 5,
    CompSizePos = 6,
    CompVersionPos = 7,
};

} // namespace fw_update

} // namespace pldm

// NOLINTEND
