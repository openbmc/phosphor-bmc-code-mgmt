#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace phosphor::software::manager
{

// Find a unique partition by its chip-relative byte offset under an SPI
// device's sysfs mtd directory. Whole flash devices are not candidates.
std::optional<std::string> findMTDPartition(
    const std::filesystem::path& mtdPath, uint64_t offset);

} // namespace phosphor::software::manager
