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
    TestEventEntry(sdbusplus::async::context& ctx,
                   const sdbusplus::object_path& path) :
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
    TestEventServer(sdbusplus::async::context& ctx,
                    const sdbusplus::object_path& path) :
        EventServerIntf(ctx, path), ctx(ctx)
    {}

    auto method_call(create_t /*unused*/, auto message, auto /*unused*/,
                     auto /*unused*/)
        -> sdbusplus::async::task<create_t::return_type>
    {
        static int cnt = 100;
        cnt++;

        auto objectPath =
            sdbusplus::object_path("/xyz/openbmc_project/logging/entry") /
            std::format("TestEvent{}", cnt);
        EXPECT_EQ(message, expectedEvent) << "Event name mismatch";

        eventEntries.emplace_back(
            std::make_unique<TestEventEntry>(ctx, objectPath));

        co_return objectPath;
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

    const sdbusplus::object_path targetObjectPath =
        sdbusplus::object_path("/xyz/openbmc_project/software/device_1234");
    static constexpr auto imageIdentifier = "v1.0";
    static constexpr auto serviceName = "xyz.openbmc_project.Logging";
    static const sdbusplus::object_path loggingPath;

    sdbusplus::async::context ctx;
    EventIntf::Events events;
    TestEventServer eventServer;
    sdbusplus::server::manager_t manager;

    FWUpdateEventsTest() :
        events(ctx), eventServer(ctx, loggingPath), manager(ctx, loggingPath)
    {
        ctx.request_name(serviceName);
    }

    auto testVerificationFailedAssertDeassert() -> sdbusplus::async::task<void>
    {
        eventServer.expectedEvent = error_intf::VerificationFailed::errName;
        co_await events.generateVerificationFailed(targetObjectPath,
                                                   imageIdentifier, true);

        EXPECT_FALSE(eventServer.eventEntries.empty())
            << "Event entry should be created after assert";
        EXPECT_FALSE(eventServer.eventEntries.back()->isResolved)
            << "Event should not be resolved after assert";

        co_await sdbusplus::async::sleep_for(ctx, 1s);

        co_await events.generateVerificationFailed(targetObjectPath,
                                                   imageIdentifier, false);

        EXPECT_TRUE(eventServer.eventEntries.back()->isResolved)
            << "Event should be resolved after deassert";

        ctx.request_stop();
    }

    auto testActivateFailedAssertDeassert() -> sdbusplus::async::task<void>
    {
        eventServer.expectedEvent = error_intf::ActivateFailed::errName;
        co_await events.generateActivateFailed(targetObjectPath,
                                               imageIdentifier, true);

        EXPECT_FALSE(eventServer.eventEntries.empty())
            << "Event entry should be created after assert";
        EXPECT_FALSE(eventServer.eventEntries.back()->isResolved)
            << "Event should not be resolved after assert";

        co_await sdbusplus::async::sleep_for(ctx, 1s);

        co_await events.generateActivateFailed(targetObjectPath,
                                               imageIdentifier, false);

        EXPECT_TRUE(eventServer.eventEntries.back()->isResolved)
            << "Event should be resolved after deassert";

        ctx.request_stop();
    }

    auto testUpdateNotApplicableAssertDeassert() -> sdbusplus::async::task<void>
    {
        eventServer.expectedEvent = error_intf::UpdateNotApplicable::errName;
        co_await events.generateUpdateNotApplicable(targetObjectPath,
                                                    imageIdentifier, true);

        EXPECT_FALSE(eventServer.eventEntries.empty())
            << "Event entry should be created after assert";
        EXPECT_FALSE(eventServer.eventEntries.back()->isResolved)
            << "Event should not be resolved after assert";

        co_await sdbusplus::async::sleep_for(ctx, 1s);

        co_await events.generateUpdateNotApplicable(targetObjectPath,
                                                    imageIdentifier, false);

        EXPECT_TRUE(eventServer.eventEntries.back()->isResolved)
            << "Event should be resolved after deassert";

        ctx.request_stop();
    }

    auto testTargetDetermined() -> sdbusplus::async::task<void>
    {
        eventServer.expectedEvent = event_intf::TargetDetermined::errName;
        co_await events.generateTargetDetermined(targetObjectPath,
                                                 imageIdentifier);

        ctx.request_stop();
    }

    auto testUpdateSuccessful() -> sdbusplus::async::task<void>
    {
        eventServer.expectedEvent = event_intf::UpdateSuccessful::errName;
        co_await events.generateUpdateSuccessful(targetObjectPath,
                                                 imageIdentifier);

        ctx.request_stop();
    }

    auto testResetRequired() -> sdbusplus::async::task<void>
    {
        eventServer.expectedEvent = event_intf::ResetRequired::errName;
        co_await events.generateResetRequired(
            targetObjectPath, EventIntf::HostTransition::Reboot);

        ctx.request_stop();
    }
};

const sdbusplus::object_path FWUpdateEventsTest::loggingPath =
    sdbusplus::object_path("/xyz/openbmc_project/logging");

TEST_F(FWUpdateEventsTest, TestVerificationFailedAssertDeassert)
{
    ctx.spawn(testVerificationFailedAssertDeassert());
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestActivateFailedAssertDeassert)
{
    ctx.spawn(testActivateFailedAssertDeassert());
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestUpdateNotApplicableAssertDeassert)
{
    ctx.spawn(testUpdateNotApplicableAssertDeassert());
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestTargetDetermined)
{
    ctx.spawn(testTargetDetermined());
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestUpdateSuccessful)
{
    ctx.spawn(testUpdateSuccessful());
    ctx.run();
}

TEST_F(FWUpdateEventsTest, TestResetRequired)
{
    ctx.spawn(testResetRequired());
    ctx.run();
}
