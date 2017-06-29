#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <phosphor-logging/log.hpp>
#include <experimental/filesystem>
#include <algorithm>
#include "config.h"
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
//#include "xyz/openbmc_project/Common/error.hpp"
//#include "xyz/openbmc_project/Object/error.hpp"
#include "delete_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

//using namespace sdbusplus::xyz::openbmc_project::Object::Error;
using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

void Delete::delete_()
{
    log<level::ERR>("LEO-------------------------   Delete:deleteImage");
    return;
}

} // namespace manager
} // namespace software
} // namespace phosphor

