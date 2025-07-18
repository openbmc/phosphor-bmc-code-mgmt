
#include "common/include/software_config.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <gtest/gtest.h>

using namespace phosphor::software;
using namespace phosphor::software::config;

const std::string objPath =
    "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

TEST(SoftwareConfig, ConfigCreate)
{
    SoftwareConfig config(objPath, 0x0324, "com.example.SampleCorp",
                          "ConfigType", "SampleComponent");

    ASSERT_EQ(config.configName, "SampleComponent");
    ASSERT_EQ(config.configType, "ConfigType");
}

TEST(SoftwareConfig, FailureCompatibleNoDot)
{
    try
    {
        SoftwareConfig config(objPath, 0x0324, "comexamplesamplecorp",
                              "ConfigType", "SampleComponent");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(SoftwareConfig, FailureCompatibleInvalidChar)
{
    try
    {
        SoftwareConfig config(objPath, 0x0324, "com-examplesamplecorp#",
                              "ConfigType", "SampleComponent");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}
