#pragma once

#include <libpldm/base.h>
#include <libpldm/utils.h>

#include <string>

// NOLINTBEGIN

namespace pldm
{
namespace utils
{

/** @brief Convert the buffer to std::string
 *
 *  If there are characters that are not printable characters, it is replaced
 *  with space(0x20).
 *
 *  @param[in] var - pointer to data and length of the data
 *
 *  @return std::string equivalent of variable field
 */
std::string toString(const struct variable_field& var);

} // namespace utils
} // namespace pldm

// NOLINTEND
