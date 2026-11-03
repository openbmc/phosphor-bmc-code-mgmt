#pragma once

#include <cstdint>
#include <string>

namespace minimum_ship_level
{

/** @brief Version components */
struct Version
{
    uint32_t major;
    uint32_t minor;
    uint32_t rev;
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

/** @brief Parse the version components using an explicit regex
 *  @param[in]  versionStr - The version string to be parsed
 *  @param[in]  pattern    - POSIX extended regex; groups 2, 3 and 4 hold the
 *                           major, minor and rev numbers
 *  @param[out] version    - The parsed version, {0, 0, 0} on failure
 *  @return true if all three components were parsed, false otherwise
 */
bool parse(const std::string& versionStr, const std::string& pattern,
           Version& version);

/** @brief Compare the versions provided
 *  @param[in] a - The first version to compare
 *  @param[in] b - The second version to compare
 *  @return 1 if a > b
 *          0 if a = b
 *         -1 if a < b
 */
int compare(const Version& a, const Version& b);

/** @brief Check if the minimum ship level option is enabled
 *  @return true if enabled, false otherwise
 */
bool enabled();

/** @brief Get the minimum version
 *  @return[out] msl - Minimum version string
 */
std::string getMinimumVersion();

} // namespace minimum_ship_level
