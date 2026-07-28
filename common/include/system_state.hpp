#pragma once

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/match.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>
#include <xyz/openbmc_project/State/OperatingSystem/Status/client.hpp>

namespace phosphor::software::system_state
{

const auto stateOn =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::HostState::Running;
const auto stateOff =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::HostState::Off;

using HostState =
    sdbusplus::client::xyz::openbmc_project::state::Host<>::HostState;

using OsState = sdbusplus::common::xyz::openbmc_project::state::
    operating_system::Status::OSStatus;

class SystemState
{
  public:
    static sdbusplus::async::task<bool> setHostState(
        sdbusplus::async::context& ctx, HostState state);

    static sdbusplus::async::task<HostState> getHostState(
        sdbusplus::async::context& ctx);

    static sdbusplus::async::task<OsState> getOsState(
        sdbusplus::async::context& ctx);

    using StateCallback = std::function<sdbusplus::async::task<>()>;

    explicit SystemState(sdbusplus::async::context& ctx);

    sdbusplus::async::task<> watchHostState(HostState expectedState,
                                            StateCallback callback);

    sdbusplus::async::task<> watchOsState(OsState expectedState,
                                          StateCallback callback);

  private:
    sdbusplus::async::task<> watchState(
        const std::string& expectedState, const char* propertyName,
        sdbusplus::async::match& match,
        std::function<sdbusplus::async::task<std::string>()> getCurrentState,
        StateCallback callback);

    sdbusplus::async::context& ctx;

    sdbusplus::async::match hostStateChangedMatch;
    sdbusplus::async::match osStateChangedMatch;
};

}; // namespace phosphor::software::system_state
