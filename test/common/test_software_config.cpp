
#include "common/include/software_config.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

const std::string objPath =
    "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

TEST(SoftwareConfig, success)
{
    SoftwareConfig config(objPath, 0x0324, "com.example.SampleCorp",
                          "ConfigType", "SampleComponent");

    ASSERT_EQ(config.vendorIANA, 0x0324);
    ASSERT_EQ(config.compatibleHardware, "com.example.SampleCorp");
}

TEST(SoftwareConfig, failureCompatibleNoDot)
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

TEST(SoftwareConfig, failureCompatibleInvalidChar)
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
