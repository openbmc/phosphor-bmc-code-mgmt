#include "config.h"

#include <sdbusplus/server.hpp>

#include <fstream>
#include <map>
#include <string>

namespace utils
{

/**
 * @brief Get the bus service
 *
 * @return the bus service as a string
 **/
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

/**
 * @brief Merge more files
 *
 * @param[in] srcFiles - source files
 * @param[out] dstFile - destination file
 * @return
 **/
void mergeFiles(std::vector<std::string>& srcFiles, std::string& dstFile);
} // namespace utils
