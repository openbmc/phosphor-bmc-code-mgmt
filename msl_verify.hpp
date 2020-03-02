#pragma once

#include <string>

namespace openpower
{
namespace software
{
namespace image
{

/** @class MinimumShipLevel
 *  @brief Contains minimum ship level verification functions.
 */
class MinimumShipLevel
{
  public:
    MinimumShipLevel() = delete;
    MinimumShipLevel(const MinimumShipLevel&) = delete;
    MinimumShipLevel& operator=(const MinimumShipLevel&) = delete;
    MinimumShipLevel(MinimumShipLevel&&) = default;
    MinimumShipLevel& operator=(MinimumShipLevel&&) = default;
    ~MinimumShipLevel() = default;

    /** @brief Constructs MinimumShipLevel.
     *  @param[in] minShipLevel - Minimum Ship Level string
     */
    explicit MinimumShipLevel(const std::string& minShipLevel) :
        minShipLevel(minShipLevel){};

    /** @brief Verify if the current BMC version meets the min ship level
     *  @return true if the verification succeeded, false otherwise
     */
    bool verify();

    /** @brief Version components */
    struct Version
    {
        uint8_t major;
        uint8_t minor;
        uint8_t rev;
    };

/** @brief Get the functional BMC version on the system
     *  
     *  @return The populated or empty version string
     */
    std::string getBMCVersion(const std::string& releaseFilePath);

    /** @brief Parse the version components into a struct
     *  @details Version format follows a git tag convention: [xx]X.Y[.Z]
     *  Depending on the maker, BMC level could be in the format of [fw]
     *  [op] [fw-] [op-] or any convination of 2 letters and/or a dash [xx-]
     *  Reference:
     *  https://github.com/open-power/op-build/blob/master/openpower/package/VERSION.readme
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

  private:
    /** Minimum Ship Level to compare against */
    std::string minShipLevel;
};

} // namespace image
} // namespace software
} // namespace openpower