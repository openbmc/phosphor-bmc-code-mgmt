#include "i2c_device_code_updater.hpp"

#include "../../common/fw_manager.hpp"
#include "vr_sw.hpp"
#include "i2c_device.hpp"


I2CDeviceCodeUpdater::I2CDeviceCodeUpdater(sdbusplus::async::context& io,
                                            bool isDryRun) :
    FWManager(io, "VR", isDryRun)
{}

std::string I2CDeviceCodeUpdater::getConfigurationInterface()
{
    return "xyz.openbmc_project.Configuraton.I2C_VR";
}

void I2CDeviceCodeUpdater::getInitialConfigurationSingleDevice(
    const std::string& service, const std::string& path)
{
    std::optional<std::string> optBusNo = 
	FWManager::dbusGetRequiredConfigurationProperty<std::string>(
	    service, path, "Bus");
    std::optional<std::string> optAddr = 
	FWManager::dbusGetRequiredConfigurationProperty<std::string>(
		service, path, "Address");
    
    std::optional<std::string> optVendorIANA =
	FWManager::dbusGetRequiredConfigurationProperty<std::string>(
		service, path, "VendorIANA");
    std::optional<std::string> optCompatible = 
	FWManager::dbusGetRequiredConfigurationProperty<std::string>(
		service, path, "Compatible");

    if !(optBusNo.has_value() || optAddr.has_value() ||
			optVendorIANA.has_value() || optCompatible.has_value()) {
		return;
    }

    lg2::debug("[config] voltage regulator device on Bus: {BUS} at Address: {ADDR}",
			"BUS","ADDR", optBusNo.value(), optAddr.value());

    std::string swid = getRandomSoftwareId();
    std::string objPathStr = getObjPathFromSwid(swid);

	// Create I2C device
	auto i2cDevice = std::make_shared<I2CDevice> (
		io, bus, address, optVendorIANA.value(), optCompatible.vallue(),
		dryRun, this);

	std::shared_ptr<VRFW> vrfw = std::make_shared<VRFW>(
		io, swid, objPathStr.c_str(), &(*i2cDevice));

	i2cDevice->softwareCurrent = vrfw;

	devices.insert(i2cDevice);
}

// NOLINTBEGIN
auto I2CDeviceCodeUpdater::getInitialConfiguration() -> sdbusplus::async::task<>
// NOLINTEND
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(io)
		.service("xyz.openbmc_project.ObjectMapper")
	.path("/xyz/openbmc_project/object_mapper");

    auto res = client.get_sub_tree("xyz/openbmc_project/inventory", 0, {});

    lg2::debug("[config] looking for dbus interface {INTF}","INTF",
	getConfigurationInterface());

    for (auto& [path, v] : res)
    {
		for (auto& [sevicename, interfaceNames] : v)
		{
	    	bool found = false;

	    	for (std::string& interfaceName : interfaceNames)
	    	{
				if (interfaceName == getConfigurationInterface())
				{
					found = true;
				}

				if (interfaceName.start_with(
		    		"xyz.openbmc_project.Configuration.VR.I2C_VR"))
				{
					// We identified an proper I2C_VR device? is that the
					// the correct place? I'm not sure.
				}
	    	}

	    	if !(found) 
	    	{
				continue;
	    	}
	    
	    	lg2::debug("[config] object path {PATH}", "PATH", path);
            lg2::debug("[config] service {SERVICE}", "SERVICE", serviceName);
            lg2::debug("[config] found I2C VR interface");
	
	    	getInitialConfigurationSingleDevice(serviceName, path);
		}
    }

    requestBusName();
}
