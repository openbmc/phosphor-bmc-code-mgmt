#include "utils.hpp"

#include <libpldm/pldm_types.h>

#include <algorithm>
#include <string>

// NOLINTBEGIN

namespace pldm
{
namespace utils
{

std::string toString(const struct variable_field& var)
{
    if (var.ptr == nullptr || !var.length)
    {
        return "";
    }

    std::string str(reinterpret_cast<const char*>(var.ptr), var.length);
    std::replace_if(
        str.begin(), str.end(), [](const char& c) { return !isprint(c); }, ' ');
    return str;
}

} // namespace utils
} // namespace pldm

// NOLINTEND
