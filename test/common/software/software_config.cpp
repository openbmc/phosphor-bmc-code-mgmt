
#include "common/include/software_config.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <gtest/gtest.h>

using namespace phosphor::software;
using namespace phosphor::software::config;

constexpr uint32_t vendorIANA = 0x0324;

constexpr const char* compatibleHardware =
    "com.ExampleCorp.Hardware.ExamplePlatform.ExampleDevice";
constexpr const char* exampleConfigName = "ExampleConfigName";
constexpr const char* exampleConfigType = "ExampleConfigType";

const std::string objPath =
    "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

TEST(SoftwareConfig, ConfigCreate)
{
    SoftwareConfig config(objPath, vendorIANA, compatibleHardware,
                          exampleConfigType, exampleConfigName);

    ASSERT_EQ(config.configName, exampleConfigName);
    ASSERT_EQ(config.configType, exampleConfigType);
}

TEST(SoftwareConfig, FailureCompatibleNoDot)
{
    try
    {
        SoftwareConfig config(objPath, vendorIANA, "comexamplesamplecorp",
                              exampleConfigType, exampleConfigName);
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(SoftwareConfig, FailureCompatibleInvalidChar)
{
    try
    {
        SoftwareConfig config(objPath, vendorIANA,
                              std::string(compatibleHardware) + "#",
                              exampleConfigType, exampleConfigName);
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}
