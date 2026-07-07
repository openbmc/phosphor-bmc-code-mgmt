
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
constexpr const char* exampleBaseInterface =
    "xyz.openbmc_project.Configuration.Example";

const std::string objPath =
    "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

TEST(SoftwareConfig, ConfigCreate)
{
    SoftwareConfig config(objPath, vendorIANA, compatibleHardware,
                          exampleConfigType, exampleConfigName,
                          exampleBaseInterface, {});

    ASSERT_EQ(config.configName, exampleConfigName);
    ASSERT_EQ(config.configType, exampleConfigType);
}

TEST(SoftwareConfig, FailureCompatibleNoDot)
{
    try
    {
        SoftwareConfig config(objPath, vendorIANA, "comexamplesamplecorp",
                              exampleConfigType, exampleConfigName,
                              exampleBaseInterface, {});
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(SoftwareConfig, FailureCompatibleInvalidChar)
{
    try
    {
        SoftwareConfig config(
            objPath, vendorIANA, std::string(compatibleHardware) + "#",
            exampleConfigType, exampleConfigName, exampleBaseInterface, {});
        ASSERT_FALSE(true);
    }
    catch (std::exception& /*unused*/)
    {}
}

TEST(SoftwareConfig, GetProperty)
{
    InterfacesMap mockInterfaces;
    DbusPropertyMap mockProperties;
    mockProperties["Bus"] = static_cast<uint64_t>(12);
    mockProperties["Name"] = std::string("TestDevice");
    mockInterfaces[exampleBaseInterface] = mockProperties;

    SoftwareConfig config(objPath, vendorIANA, compatibleHardware,
                          exampleConfigType, exampleConfigName,
                          exampleBaseInterface, mockInterfaces);

    auto bus = config.getProperty<uint64_t>("Bus");
    ASSERT_TRUE(bus.has_value());
    ASSERT_EQ(bus.value(), 12);

    auto name = config.getProperty<std::string>("Name");
    ASSERT_TRUE(name.has_value());
    ASSERT_EQ(name.value(), "TestDevice");

    // non-exist property
    auto missing = config.getProperty<uint64_t>("MissingKey");
    ASSERT_FALSE(missing.has_value());

    // wrong property type
    auto wrongType = config.getProperty<std::string>("Bus");
    ASSERT_FALSE(wrongType.has_value());
}
