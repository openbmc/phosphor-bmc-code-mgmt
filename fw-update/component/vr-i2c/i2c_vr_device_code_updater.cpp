#include "i2c_vr_device_code_updater.hpp"

#include "fw-update/common/include/fw_manager.hpp"
#include "i2c_vr_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <cstdint>

const std::string configTypeI2CVRDevice = "VR";

I2CVRDeviceCodeUpdater::I2CVRDeviceCodeUpdater(sdbusplus::async::context& io,
                                               bool isDryRun, bool debug) :
    FWManager(io, configTypeI2CVRDevice, isDryRun, debug)
{}

void I2CVRDeviceCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& service, const std::string& path)
{
    std::optional<uint32_t> optBusNo =
        FWManager::dbusGetRequiredConfigurationProperty<uint32_t>(
            service, path, "Bus");
    std::optional<uint32_t> optAddr =
        FWManager::dbusGetRequiredConfigurationProperty<uint32_t>(
            service, path, "Address");
    std::optional<std::string> optVRChipType =
        FWManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Type");
    std::optional<uint64_t> optVendorIANA =
        FWManager::dbusGetRequiredConfigurationProperty<uint64_t>(
            service, path, "VendorIANA");
    std::optional<std::string> optCompatible =
        FWManager::dbusGetRequiredConfigurationProperty<std::string>(
            service, path, "Compatible");

    if (!optBusNo.has_value() || !optAddr.has_value() ||
        !optVRChipType.has_value() || !optVendorIANA.has_value() ||
        !optCompatible.has_value())
    {
        return;
    }

    lg2::debug(
        "[config] voltage regulator device type: {TYPE} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", optVRChipType.value(), "BUS", optBusNo.value(), "ADDR",
        optAddr.value());

    uint32_t vendorIANA = 0;
    try
    {
        vendorIANA = optVendorIANA.value();
    }
    catch (std::exception& e)
    {
        lg2::error(e.what());
        return;
    }

    sdbusplus::async::context& io = this->getIOContext();

    auto i2cDevice = std::make_unique<I2CVRDevice>(
        io, dryRun, optVRChipType.value(), optBusNo.value(), optAddr.value(),
        vendorIANA, optCompatible.value(), path, this);

    std::unique_ptr<Software> vrfw =
        std::make_unique<Software>(io, path.c_str(), dryRun, *i2cDevice);

    vrfw->enableUpdate();

    i2cDevice->softwareCurrent = std::move(vrfw);

    devices.insert(std::move(i2cDevice));
}

// NOLINTBEGIN
auto I2CVRDeviceCodeUpdater::getInitialConfiguration()
    -> sdbusplus::async::task<>
// NOLINTEND
{
    sdbusplus::async::context& io = this->getIOContext();

    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(io)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    auto res =
        co_await client.get_sub_tree("xyz/openbmc_project/inventory", 0, {});

    lg2::debug("[config] looking for dbus interface {INTF}", "INTF",
               getConfigurationInterface());

    for (auto& [path, v] : res)
    {
        for (auto& [servicename, interfaceNames] : v)
        {
            bool found = false;

            for (std::string& interfaceName : interfaceNames)
            {
                if (interfaceName == getConfigurationInterface())
                {
                    found = true;
                }

                if (interfaceName.starts_with(
                        "xyz.openbmc_project.Configuration.VR.I2C_VR"))
                {
                    // We identified an proper I2C_VR device? is that the
                    // the correct place? I'm not sure.
                }
            }

            if (!found)
            {
                continue;
            }

            lg2::debug("[config] object path {PATH}", "PATH", path);
            lg2::debug("[config] service {SERVICE}", "SERVICE", servicename);
            lg2::debug("[config] found I2C VR interface");

            getInitialConfigurationSingleDevice(servicename, path);
        }
    }

    requestBusName();
}
