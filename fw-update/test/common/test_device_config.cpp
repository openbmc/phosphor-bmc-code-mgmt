
#include "fw-update/common/include/device_config.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

TEST(DeviceConfig, success)
{
    DeviceConfig config(0x0324, "com.example.SampleCorp", "ConfigType",
                        "/xyz/openbmc_project/inventory/system/s1/mycomponent");

    ASSERT_EQ(config.vendorIANA, 0x0324);
    ASSERT_EQ(config.compatible, "com.example.SampleCorp");
    ASSERT_EQ(config.objPathConfig,
              "/xyz/openbmc_project/inventory/system/s1/mycomponent");
}

TEST(DeviceConfig, failureCompatibleNoDot)
{
    try
    {
        DeviceConfig config(
            0x0324, "comexamplesamplecorp", "ConfigType",
            "/xyz/openbmc_project/inventory/system/s1/mycomponent");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(DeviceConfig, failureCompatibleInvalidChar)
{
    try
    {
        DeviceConfig config(
            0x0324, "com-examplesamplecorp#", "ConfigType",
            "/xyz/openbmc_project/inventory/system/s1/mycomponent");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(DeviceConfig, failureObjPath)
{
    try
    {
        DeviceConfig config(0x0324, "com.example.samplecorp", "ConfigType",
                            "noslash");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}
