#include "mtd_utils.hpp"

#include <charconv>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>

namespace phosphor::software::manager
{
namespace
{

bool isMTDDevice(std::string_view name)
{
    return name.starts_with("mtd") && name.size() > 3 &&
           name.find_first_not_of("0123456789", 3) == std::string_view::npos;
}

std::optional<uint64_t> readOffset(const std::filesystem::path& path)
{
    std::ifstream file(path / "offset");
    std::string text;
    if (!std::getline(file, text))
    {
        return std::nullopt;
    }

    uint64_t offset = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), offset);
    if (error != std::errc{} || end != text.data() + text.size())
    {
        return std::nullopt;
    }
    return offset;
}

std::optional<uint64_t> getChipOffset(const std::filesystem::path& partition,
                                      const std::filesystem::path& mtdPath)
{
    auto offset = readOffset(partition);
    if (!offset.has_value())
    {
        return std::nullopt;
    }

    // The kernel reports offsets relative to the immediate parent MTD device.
    for (auto parent = partition.parent_path(); parent != mtdPath;
         parent = parent.parent_path())
    {
        if (!isMTDDevice(parent.filename().string()))
        {
            return std::nullopt;
        }

        auto parentOffset = readOffset(parent);
        if (!parentOffset.has_value())
        {
            std::error_code error;
            if (parent.parent_path() == mtdPath &&
                !std::filesystem::exists(parent / "offset", error) && !error)
            {
                // A partitioned master has no offset attribute.
                continue;
            }
            return std::nullopt;
        }
        if (parentOffset.value() >
            std::numeric_limits<uint64_t>::max() - offset.value())
        {
            return std::nullopt;
        }
        offset.value() += parentOffset.value();
    }
    return offset;
}

} // namespace

std::optional<std::string> findMTDPartition(
    const std::filesystem::path& mtdPath, uint64_t offset)
{
    auto root = mtdPath.lexically_normal();
    if (root.filename().empty())
    {
        root = root.parent_path();
    }
    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(root, error);
    const std::filesystem::recursive_directory_iterator end;
    std::optional<std::string> device;

    while (!error && entry != end)
    {
        const auto name = entry->path().filename().string();
        if (!isMTDDevice(name))
        {
            entry.disable_recursion_pending();
        }
        else if (entry->is_directory(error) && !entry->is_symlink(error))
        {
            const auto candidate = getChipOffset(entry->path(), root);
            if (candidate.has_value() && candidate.value() == offset)
            {
                if (device.has_value())
                {
                    return std::nullopt;
                }
                device = "/dev/" + name;
            }
        }
        if (error)
        {
            return std::nullopt;
        }
        entry.increment(error);
    }

    if (error)
    {
        return std::nullopt;
    }
    return device;
}

} // namespace phosphor::software::manager
