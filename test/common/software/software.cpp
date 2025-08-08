#include "../exampledevice/example_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

#include <memory>
#include <regex>

#include <gtest/gtest.h>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::example_device;

class SoftwareTest : public testing::Test
{
  protected:
    SoftwareTest() :
        exampleUpdater(ctx, true, nullptr), device(exampleUpdater.getDevice())
    {}
    ~SoftwareTest() noexcept override {}

    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater;
    std::unique_ptr<ExampleDevice>& device;

  public:
    SoftwareTest(const SoftwareTest&) = delete;
    SoftwareTest(SoftwareTest&&) = delete;
    SoftwareTest& operator=(const SoftwareTest&) = delete;
    SoftwareTest& operator=(SoftwareTest&&) = delete;
};

TEST_F(SoftwareTest, TestSoftwareConstructor)
{
    // the software version is currently unknown
    EXPECT_EQ(device->softwareCurrent, nullptr);

    auto sw = std::make_unique<Software>(ctx, *device);

    // since that software is not an update, there is no progress
    EXPECT_EQ(sw->softwareActivationProgress, nullptr);
}

TEST_F(SoftwareTest, TestVersionPurpose)
{
    auto sw = std::make_unique<ExampleSoftware>(ctx, *device);

    EXPECT_EQ(sw->getPurpose(), std::nullopt);

    sw->setVersion("swVersion");
    EXPECT_EQ(sw->getPurpose(), SoftwareVersion::VersionPurpose::Unknown);
}

TEST_F(SoftwareTest, TestSoftwareId)
{
    auto sw = std::make_unique<Software>(ctx, *device);

    std::regex re("ExampleSoftware_[0-9]+");
    std::cmatch m;

    // design: Swid = <DeviceX>_<RandomId>
    EXPECT_TRUE(std::regex_match(sw->swid.c_str(), m, re));
}

TEST_F(SoftwareTest, TestSoftwareObjectPath)
{
    auto sw = std::make_unique<ExampleSoftware>(ctx, *device);

    debug("{PATH}", "PATH", sw->objectPath);

    // assert that the object path is as per the design
    // design: /xyz/openbmc_project/Software/<SwId>
    EXPECT_TRUE(std::string(sw->objectPath)
                    .starts_with("/xyz/openbmc_project/software/"));
}
