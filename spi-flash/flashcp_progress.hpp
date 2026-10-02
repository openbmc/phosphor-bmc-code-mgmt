#pragma once

#include <optional>
#include <string>
#include <string_view>

inline std::optional<unsigned> parseFlashcpProgress(std::string_view line)
{
    // Example flashcp progress output:
    // "Erasing block: 50/100 (50%)"
    // "Writing kb: 512/1024 (50%)"
    // "Verifying kb: 512/1024 (50%)"
    // Extract the percentage between '(' and '%'.

    auto left = line.find('(');
    auto right = line.find('%');

    if (left == std::string_view::npos || right == std::string_view::npos ||
        right <= left)
    {
        return std::nullopt;
    }

    int percent = 0;

    try
    {
        percent =
            std::stoi(std::string(line.substr(left + 1, right - left - 1)));
    }
    catch (...)
    {
        return std::nullopt;
    }

    if (percent < 0 || percent > 100)
    {
        return std::nullopt;
    }

    if (line.starts_with("Erasing block"))
    {
        return 30U + static_cast<unsigned>(percent) * 15U / 100U;
    }

    if (line.starts_with("Writing kb"))
    {
        return 45U + static_cast<unsigned>(percent) * 35U / 100U;
    }

    if (line.starts_with("Verifying kb"))
    {
        return 80U + static_cast<unsigned>(percent) * 10U / 100U;
    }

    return std::nullopt;
}
