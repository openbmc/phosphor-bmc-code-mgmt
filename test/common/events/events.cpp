// This test verifies that firmware update events and errors are correctly
// committed to the phosphor-logging service via lg2::commit, and that
// errors can be resolved via lg2::resolve when deasserted.
#include "common/include/events.hpp"

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Logging/Create/aserver.hpp>
#include <xyz/openbmc_project/Logging/Entry/aserver.hpp>
#include <xyz/openbmc_project/Software/Update/event.hpp>

#include <gtest/gtest.h>

using namespace std::literals;
namespace EventIntf = phosphor::software::events;

class TestEventServer;
class TestEventEntry;

using EventServerIntf =
    sdbusplus::aserver::xyz::openbmc_project::logging::Create<TestEventServer>;
using EventEntryIntf =
    sdbusplus::aserver::xyz::openbmc_project::logging::Entry<TestEventEntry>;

namespace error_intf = sdbusplus::error::xyz::openbmc_project::software::Update;
namespace event_intf = sdbusplus::event::xyz::openbmc_project::software::Update;

// Mock log entry that implements the Logging.Entry interface.
// Created by TestEventServer for each committed event.
class TestEventEntry : public EventEntryIntf
{
  public:
    TestEventEntry(sdbusplus::async::context& ctx, const char* path) :
        EventEntryIntf(ctx, path)
    {}

    static auto method_call(get_entry_t /*unused*/)
        -> sdbusplus::async::task<get_entry_t::return_type>
    {
        get_entry_t::return_type fd1 = 0;
        co_return fd1;
    }

    auto set_property(resolved_t /*unused*/, bool value) -> bool
    {
        bool changed = (isResolved != value);
        isResolved = value;
        return changed;
    }

    bool isResolved = false;
};

// Mock logging service that implements xyz.openbmc_project.Logging.Create
// to intercept lg2::commit calls. On each Create call, verifies that the
// event name matches expectedEvent and returns a unique log entry object path.
class TestEventServer : public EventServerIntf
{
  public:
    TestEventServer(sdbusplus::async::context& ctx, const char* path) :
        EventServerIntf(ctx, path), ctx(ctx)
    {}

    auto method_call(create_t /*unused*/, auto message, auto /*unused*/,
                     auto /*unused*/)
        -> sdbusplus::async::task<create_t::return_type>
    {
        static int cnt = 100;
        cnt++;

        std::string objectPath =
            "/xyz/openbmc_project/logging/entry/TestEvent" +
            std::to_string(cnt);
        EXPECT_EQ(message, expectedEvent) << "Event name mismatch";

        eventEntries.emplace_back(
            std::make_unique<TestEventEntry>(ctx, objectPath.c_str()));

        co_return sdbusplus::object_path(objectPath);
    }

    auto method_call(create_with_ffdc_files_t /*unused*/, auto /*unused*/,
                     auto /*unused*/, auto /*unused*/, auto /*unused*/)
        -> sdbusplus::async::task<create_with_ffdc_files_t::return_type>
    {
        co_return;
    }

    std::string expectedEvent;
    std::vector<std::unique_ptr<TestEventEntry>> eventEntries;

  private:
    sdbusplus::async::context& ctx;
};

class FWUpdateEventsTest : public ::testing::Test
{
  public:
    FWUpdateEventsTest(const FWUpdateEventsTest&) = delete;
    FWUpdateEventsTest(FWUpdateEventsTest&&) = delete;
    FWUpdateEventsTest& operator=(const FWUpdateEventsTest&) = delete;
    FWUpdateEventsTest& operator=(FWUpdateEventsTest&&) = delete;
    ~FWUpdateEventsTest() noexcept override = default;

    enum class EventTestType
    {
        verificationFailed,
        applyFailed,
        targetDetermined,
        updateSuccessful,
        resetRequired,
    };

    static constexpr auto targetObjectPath =
        "/xyz/openbmc_project/software/device_1234";
    static constexpr auto imageIdentifier = "v1.0";
    static constexpr auto serviceName = "xyz.openbmc_project.Logging";
    static constexpr auto assert = true;
    static constexpr auto deassert = false;
    const char* loggingPath = "/xyz/openbmc_project/logging";

    sdbusplus::async::context ctx;
    EventIntf::Events events;
    TestEventServer eventServer;
    sdbusplus::server::manager_t manager;

    FWUpdateEventsTest() :
        events(ctx), eventServer(ctx, loggingPath), manager(ctx, loggingPath)
    {
        ctx.request_name(serviceName);
    }

    // Assert an event: sets the expected event name on the mock server,
    // then calls the corresponding generate method which triggers
    // lg2::commit. The mock server verifies the event name matches.
    auto testAssertEvent(EventTestType eventType)
        -> sdbusplus::async::task<void>
    {
        switch (eventType)
        {
            case EventTestType::verificationFailed:
                eventServer.expectedEvent =
                    error_intf::VerificationFailed::errName;
                co_await events.generateVerificationFailed(
                    sdbusplus::object_path(targetObjectPath), imageIdentifier,
                    assert);
                break;
            case EventTestType::applyFailed:
                eventServer.expectedEvent = error_intf::ApplyFailed::errName;
                co_await events.generateApplyFailed(
                    sdbusplus::object_path(targetObjectPath), imageIdentifier,
                    assert);
                break;
            case EventTestType::targetDetermined:
                eventServer.expectedEvent =
                    event_intf::TargetDetermined::errName;
                co_await events.generateTargetDetermined(
                    sdbusplus::object_path(targetObjectPath), imageIdentifier);
                break;
            case EventTestType::updateSuccessful:
                eventServer.expectedEvent =
                    event_intf::UpdateSuccessful::errName;
                co_await events.generateUpdateSuccessful(
                    sdbusplus::object_path(targetObjectPath), imageIdentifier);
                break;
            case EventTestType::resetRequired:
                eventServer.expectedEvent = event_intf::ResetRequired::errName;
                co_await events.generateResetRequired(
                    sdbusplus::object_path(targetObjectPath),
                    EventIntf::HostTransition::Reboot);
                break;
        }
    }

    // Deassert an error event: calls the generate method with
    // asserted=false, which triggers lg2::resolve on the pending event.
    auto testDeassertEvent(EventTestType eventType)
        -> sdbusplus::async::task<void>
    {
        switch (eventType)
        {
            case EventTestType::verificationFailed:
                co_await events.generateVerificationFailed(
                    sdbusplus::object_path(targetObjectPath), imageIdentifier,
                    deassert);
                break;
            case EventTestType::applyFailed:
                co_await events.generateApplyFailed(
                    sdbusplus::object_path(targetObjectPath), imageIdentifier,
                    deassert);
                break;
            default:
                break;
        }
    }

    // Test the full assert/deassert cycle for error events.
    auto testErrorAssertDeassert(EventTestType eventType)
        -> sdbusplus::async::task<void>
    {
        co_await testAssertEvent(eventType);

        EXPECT_FALSE(eventServer.eventEntries.empty())
            << "Event entry should be created after assert";
        EXPECT_FALSE(eventServer.eventEntries.back()->isResolved)
            << "Event should not be resolved after assert";

        co_await sdbusplus::async::sleep_for(ctx, 1s);

        co_await testDeassertEvent(eventType);

        EXPECT_TRUE(eventServer.eventEntries.back()->isResolved)
            << "Event should be resolved after deassert";

        ctx.request_stop();
    }

    // Test a single commit for informational events.
    auto testEventCommit(EventTestType eventType)
        -> sdbusplus::async::task<void>
    {
        co_await testAssertEvent(eventType);

        ctx.request_stop();
    }
};

TEST_F(FWUpdateEventsTest, TestVerificationFailedAssertDeassert)
{
    ctx.spawn(testErrorAssertDeassert(EventTestType::verificationFailed));
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestApplyFailedAssertDeassert)
{
    ctx.spawn(testErrorAssertDeassert(EventTestType::applyFailed));
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestTargetDetermined)
{
    ctx.spawn(testEventCommit(EventTestType::targetDetermined));
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestUpdateSuccessful)
{
    ctx.spawn(testEventCommit(EventTestType::updateSuccessful));
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestResetRequired)
{
    ctx.spawn(testEventCommit(EventTestType::resetRequired));
    ctx.run();
}
