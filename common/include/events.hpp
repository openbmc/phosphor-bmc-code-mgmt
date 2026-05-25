#pragma once

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/task.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <map>
#include <string>

namespace phosphor::software::events
{

using HostTransition =
    sdbusplus::common::xyz::openbmc_project::state::Host::Transition;

class Events
{
  public:
    Events() = delete;

    explicit Events(sdbusplus::async::context& ctx) : ctx(ctx) {}

    auto generateVerificationFailed(sdbusplus::object_path targetName,
                                    std::string imageIdentifier, bool asserted)
        -> sdbusplus::async::task<>;

    auto generateActivateFailed(sdbusplus::object_path targetName,
                                std::string imageIdentifier, bool asserted)
        -> sdbusplus::async::task<>;

    auto generateTargetDetermined(sdbusplus::object_path targetName,
                                  std::string imageIdentifier)
        -> sdbusplus::async::task<>;

    auto generateUpdateSuccessful(sdbusplus::object_path targetName,
                                  std::string imageIdentifier)
        -> sdbusplus::async::task<>;

    auto generateResetRequired(sdbusplus::object_path targetName,
                               HostTransition resetType)
        -> sdbusplus::async::task<>;

  private:
    using event_map_t = std::map<std::string, sdbusplus::object_path>;

    template <typename ErrorType>
    auto generateError(std::string_view errorName,
                       sdbusplus::object_path targetName,
                       std::string imageIdentifier, bool asserted)
        -> sdbusplus::async::task<>;

    template <typename EventType>
    auto generateEvent(std::string_view eventName,
                       sdbusplus::object_path targetName,
                       std::string imageIdentifier) -> sdbusplus::async::task<>;

    sdbusplus::async::context& ctx;
    event_map_t pendingEvents;
};

} // namespace phosphor::software::events
