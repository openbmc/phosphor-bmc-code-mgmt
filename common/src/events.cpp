#include "events.hpp"

#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Software/Update/event.hpp>

namespace phosphor::software::events
{

PHOSPHOR_LOG2_USING;

namespace error_intf = sdbusplus::error::xyz::openbmc_project::software::Update;
namespace event_intf = sdbusplus::event::xyz::openbmc_project::software::Update;

// Strip the unique suffix (e.g. "_1234") from the object path so the
// pendingEvents key remains stable across update attempts.
static auto getDeviceBaseObjPath(const sdbusplus::object_path& path)
    -> std::string
{
    auto pos = path.str.rfind('_');
    if (pos != std::string::npos)
    {
        return path.str.substr(0, pos);
    }
    return path.str;
}

template <typename ErrorType>
auto Events::generateError(std::string_view errorName,
                           sdbusplus::object_path targetName,
                           std::string imageIdentifier, bool asserted)
    -> sdbusplus::async::task<>
{
    auto eventName =
        getDeviceBaseObjPath(targetName) + "." + std::string(errorName);
    auto pendingEvent = pendingEvents.find(eventName);

    if (asserted)
    {
        try
        {
            auto eventPath = co_await lg2::commit(
                ctx, ErrorType("IMAGE_IDENTIFIER", imageIdentifier,
                               "TARGET_NAME", targetName));
            pendingEvents[eventName] = eventPath;
        }
        catch (const std::exception& e)
        {
            error("Failed to commit {NAME} event: {ERR}", "NAME", errorName,
                  "ERR", e.what());
        }
    }
    else
    {
        if (pendingEvent != pendingEvents.end())
        {
            try
            {
                co_await lg2::resolve(ctx, pendingEvent->second);
            }
            catch (const std::exception& e)
            {
                error("Failed to resolve {NAME} event: {ERR}", "NAME",
                      errorName, "ERR", e.what());
            }
            pendingEvents.erase(eventName);
        }
    }

    debug("{NAME} event for {KEY} is {STATUS}", "NAME", errorName, "KEY",
          eventName, "STATUS", (asserted ? "asserted" : "deasserted"));
}

auto Events::generateVerificationFailed(sdbusplus::object_path targetName,
                                        std::string imageIdentifier,
                                        bool asserted)
    -> sdbusplus::async::task<>
{
    co_await generateError<error_intf::VerificationFailed>(
        "VerificationFailed", targetName, imageIdentifier, asserted);
}

auto Events::generateActivateFailed(sdbusplus::object_path targetName,
                                    std::string imageIdentifier, bool asserted)
    -> sdbusplus::async::task<>
{
    co_await generateError<error_intf::ActivateFailed>(
        "ActivateFailed", targetName, imageIdentifier, asserted);
}

template <typename EventType>
auto Events::generateEvent(std::string_view eventName,
                           sdbusplus::object_path targetName,
                           std::string imageIdentifier)
    -> sdbusplus::async::task<>
{
    try
    {
        co_await lg2::commit(ctx,
                             EventType("TARGET_NAME", targetName,
                                       "IMAGE_IDENTIFIER", imageIdentifier));
    }
    catch (const std::exception& e)
    {
        error("Failed to commit {NAME} event: {ERR}", "NAME", eventName, "ERR",
              e.what());
    }
}

auto Events::generateTargetDetermined(sdbusplus::object_path targetName,
                                      std::string imageIdentifier)
    -> sdbusplus::async::task<>
{
    co_await generateEvent<event_intf::TargetDetermined>(
        "TargetDetermined", targetName, imageIdentifier);
}

auto Events::generateUpdateSuccessful(sdbusplus::object_path targetName,
                                      std::string imageIdentifier)
    -> sdbusplus::async::task<>
{
    co_await generateEvent<event_intf::UpdateSuccessful>(
        "UpdateSuccessful", targetName, imageIdentifier);
}

auto Events::generateResetRequired(sdbusplus::object_path targetName,
                                   HostTransition resetType)
    -> sdbusplus::async::task<>
{
    try
    {
        co_await lg2::commit(
            ctx, event_intf::ResetRequired("TARGET_NAME", targetName,
                                           "RESET_TYPE", resetType));
    }
    catch (const std::exception& e)
    {
        error("Failed to commit ResetRequired event: {ERR}", "ERR", e.what());
    }
}

} // namespace phosphor::software::events
