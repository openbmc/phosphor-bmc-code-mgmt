#include <iostream>
#include <log.hpp>
#include "level_software_manager.hpp"

namespace phosphor
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

void Level::setInitialProperties(const std::string& version,
    const LevelPurpose& purpose)
{
    server::Level::purpose(purpose);

    server::Level::version(version);

    return;
}

std::string Level::version(const std::string value)
{
    log<level::INFO>("Change to Software Version",
                     entry("VERSION=%s",
                           value));

    return server::Level::version(value);
}


Level::LevelPurpose Level::purpose(const LevelPurpose value)
{
    log<level::INFO>("Change to Software Purpose",
                     entry("PURPOSE=%s",
                           convertForMessage(value).c_str()));

    return server::Level::purpose(value);
}

} // namespace manager
} // namespace software
} // namepsace phosphor
