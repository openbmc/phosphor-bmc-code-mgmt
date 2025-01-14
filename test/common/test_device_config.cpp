
#include "common/include/device_config.hpp"

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
                        "SampleComponent");

    ASSERT_EQ(config.vendorIANA, 0x0324);
    ASSERT_EQ(config.compatibleHardware, "com.example.SampleCorp");
}

TEST(DeviceConfig, failureCompatibleNoDot)
{
    try
    {
        DeviceConfig config(0x0324, "comexamplesamplecorp", "ConfigType",
                            "SampleComponent");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(DeviceConfig, failureCompatibleInvalidChar)
{
    try
    {
        DeviceConfig config(0x0324, "com-examplesamplecorp#", "ConfigType",
                            "SampleComponent");
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}
