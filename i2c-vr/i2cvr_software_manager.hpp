#pragma once

#include "common/include/software_manager.hpp"

#include <sdbusplus/async/context.hpp>

namespace ManagerInf = phosphor::software::manager;
namespace SDBusAsync = sdbusplus::async;

class I2CVRSoftwareManager : public ManagerInf::SoftwareManager
{
  public:
    I2CVRSoftwareManager(SDBusAsync::context& ctx);

    SDBusAsync::task<bool> initDevice(const std::string& service,
                                      const std::string& path,
                                      SoftwareConfig& config) final;

    void start();
};
