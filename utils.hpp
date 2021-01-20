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

namespace internal
{

int executeCmd(const char* path, char** args);

} // namespace internal

/**
 * @brief Execute command in child process
 * @param[in] path - Fully qualified name of the executable to run
 * @param[in] args - Optional arguments
 */
template <typename... Arg>
int execute(const char* path, Arg&&... arg)
{
    using argType = char*[];

    // exec requires the name of the executable to be the first argument, use
    // the path name followed by the provided arguments to build the arg list
    argType args = {const_cast<char*>(path), const_cast<char*>(arg)...,
                    nullptr};

    return internal::executeCmd(path, args);
}

} // namespace utils
