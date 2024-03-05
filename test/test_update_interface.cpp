#include "image_manager.hpp"

#include <fcntl.h>

#include <sdbusplus/test/sdbus_mock.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <gtest/gtest.h>

namespace ManagerIntf = phosphor::software::manager;
using UpdateIntf = sdbusplus::xyz::openbmc_project::Software::server::Update;

class UpdateInterfaceTest : public ::testing::Test
{
  public:
    sdbusplus::SdBusMock sdbusMock;
    sdbusplus::bus_t bus = sdbusplus::get_mocked_new(&sdbusMock);
    static constexpr auto busName = "xyz.openbmc_project.test.SoftwareManager";
    static constexpr auto objPath = "/xyz/openbmc_project/software/test";

    void SetUp() override
    {
        sdbusplus::server::manager_t objManager(bus, objPath);
        bus.request_name(busName);
    }
};

TEST_F(UpdateInterfaceTest, TestCreation)
{
    auto softwareManager = std::make_unique<ManagerIntf::Manager>(bus);
    const char* filename = "/tmp/image.tar.gz";
    const int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        throw std::runtime_error("Failed to open Image file");
    }
    auto versionPath = softwareManager->startUpdate(
        fd, UpdateIntf::ApplyTimes::Immediate, false);
}
