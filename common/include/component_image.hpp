#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace phosphor::software::device
{

// One component image extracted from the update package, in package order.
struct ComponentImage
{
    // Bytes of this component image. The span points into the caller-owned
    // mmap'd package buffer and is only valid for that buffer's lifetime.
    std::span<const uint8_t> image;

    // ComponentVersionString from the package's component image
    // information area (DSP0267): the version of the new component image
    // carried in the package - not the package version and not the
    // version currently running on the device.
    std::string version;
};

} // namespace phosphor::software::device
