#pragma once

#include "common/include/software_manager.hpp"

namespace ManagerInf = phosphor::software::manager;

const std::string configTypeTPM = "TPM";

class TPMSoftwareManager : public ManagerInf::SoftwareManager
{
  public:
    TPMSoftwareManager(sdbusplus::async::context& ctx) :
        SoftwareManager(ctx, configTypeTPM)
    {}

    void start();

    sdbusplus::async::task<bool> initDevice(const std::string& service,
                                            const std::string& path,
                                            SoftwareConfig& config) final;
};
