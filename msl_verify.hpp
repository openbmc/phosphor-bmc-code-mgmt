#pragma once

#include <string>

namespace minimum_ship_level
{

/** @brief Version components */
struct Version
{
    uint8_t major;
    uint8_t minor;
    uint8_t rev;
};

/** @brief Verify if the current BMC version meets the min ship level
 *  @return true if the verification succeeded, false otherwise
 */
bool verify(const std::string& versionStr);

/** @brief Parse the version components into a struct
 *  @details User passes a version string in regex format (REGEX_BMC_MSL)
 *  at compilation time, this value is break down by parse function to allocate
 *  a struct so it can be compared position by position against the (BMC_MSL)
 *  also defined at compile time.
 * @param[in]  versionStr - The version string to be parsed
 * @param[out] version    - The version struct to be populated
 */
void parse(const std::string& versionStr, Version& version);

/** @brief Compare the versions provided
 *  @param[in] a - The first version to compare
 *  @param[in] b - The second version to compare
 *  @return 1 if a > b
 *          0 if a = b
 *         -1 if a < b
 */
int compare(const Version& a, const Version& b);

} // namespace minimum_ship_level
