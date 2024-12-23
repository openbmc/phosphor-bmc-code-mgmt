#pragma once

#include "version_provider.hpp"

namespace phosphor::software::eeprom::version
{

class PT5161LVersionProvider : public VersionProvider
{
  public:
    using VersionProvider::VersionProvider;
    std::string getVersion() final;
    bool isHostOnRequiredToGetVersion() final;
};

} // namespace phosphor::software::eeprom::version
