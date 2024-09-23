#include <fstream>

#include "phosphor-logging/lg2.hpp"
#include <sdbusplus/server.hpp>

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

#include "spi_device_code_updater.hpp"
#include "bios_sw.hpp"

SPIDeviceCodeUpdater::SPIDeviceCodeUpdater(const std::shared_ptr<sdbusplus::asio::connection>& conn, boost::asio::io_context& io, bool isDryRun): CodeUpdater(conn, io, "BIOS", isDryRun)
{
	//TODO(design): get device access info from xyz/openbmc_project/inventory/system/... path

	//Swid = <DeviceX>_<RandomId>
	std::string swid = "first_sw_version_" + getRandomSoftwareId();

	//(design): Create Interface xyz.openbmc_project.Software.Update at /xyz/openbmc_project/Software/<SwId>
	//(design): Create Interfacexyz.openbmc_project.Software.Activation at /xyz/openbmc_project/Software/<SwId>
	// with Status = Active

	//TODO(design): Create functional association from Version to Inventory Item

	std::string objPathStr = getObjPath(swid);
	const char* objPath = objPathStr.c_str();

	std::shared_ptr<BiosSW> bsws = std::make_shared<BiosSW>(bus, swid, objPath, this);

	softwares[swid] = bsws;
}

bool SPIDeviceCodeUpdater::verifyImage(sdbusplus::message::unix_fd image)
{
	(void)image;
	// TODO(design: verify image
	lg2::info("verifying image...");
	return true;
}

void SPIDeviceCodeUpdater::startUpdateAsync(const boost::system::error_code& error){

	(void)error;
	// design: generate a new random id for the new fw image.
	// this new swid will then be used for the object path for the new image

	std::string newSwId = "newswid_version_" + getRandomSoftwareId();
	std::string objPathStr = getObjPath(newSwId);
	const char* objPath = objPathStr.c_str();

	// create the new instance of the BiosSW class
	std::shared_ptr<BiosSW> newsw = std::make_shared<BiosSW>(bus, newSwId, objPath, this);

	softwares[newSwId] = newsw;

	continueUpdate(image, applyTime, oldSwId, newSwId);
}

void SPIDeviceCodeUpdater::bindSPIFlash(const std::string& spi_dev)
{
	std::ofstream ofbind("/sys/bus/platform/drivers/spi-aspeed-smc/bind", std::ofstream::out);
	ofbind << spi_dev;
	ofbind.close();
	lg2::info("SPI flash bound to SMC");
}

void SPIDeviceCodeUpdater::unbindSPIFlash(const std::string& spi_dev)
{
	std::ofstream ofunbind("/sys/bus/platform/drivers/spi-aspeed-smc/unbind", std::ofstream::out);
	ofunbind << spi_dev;
	ofunbind.close();
	lg2::info("SPI flash unbound from SMC");
}

void SPIDeviceCodeUpdater::writeSPIFlash(sdbusplus::message::unix_fd image)
{
	//Q: do we power off the machine or just throw an error if it's not off
	bool success = CodeUpdater::setHostPowerstate(false);

	if (!success){
		lg2::error("error changing host state");
		return;
	}

	if (dryRun) {
		lg2::info("dry run, NOT writing to the chip");
	}else{
		// TODO: select the spi flash via mux / GPIO

		// TODO(s5549): set gpio BMC_SPI_SEL=1

		// Patrick mentioned this requirement
		const std::string SPI_DEV = "1e630000.spi";

		SPIDeviceCodeUpdater::bindSPIFlash(SPI_DEV);

		//call flashrom
		std::filesystem::copy("/proc/self/fd/"+std::to_string(image.fd), "/tmp/image");

		std::string cmd = std::format("flashrom -p linux_mtd:dev=6 -w {}", "/tmp/image");
		const int exitcode = system(cmd.c_str());

		if (exitcode != 0){
			lg2::error("flashrom failed to write the image");
		}

		SPIDeviceCodeUpdater::unbindSPIFlash(SPI_DEV);

		// TODO(s5549): set gpio BMC_SPI_SEL=0

		//TODO: switch bios flash back to host via mux / GPIO
	}

	CodeUpdater::setHostPowerstate(true);
}


//const auto actActive = sdbusplus::common::xyz::openbmc_project::software::Activation::Activations::Active;
const auto actNotReady = sdbusplus::common::xyz::openbmc_project::software::Activation::Activations::NotReady;
const auto actInvalid = sdbusplus::common::xyz::openbmc_project::software::Activation::Activations::Invalid;
const auto actReady = sdbusplus::common::xyz::openbmc_project::software::Activation::Activations::Ready;
const auto actActivating = sdbusplus::common::xyz::openbmc_project::software::Activation::Activations::Activating;

const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::software::ApplyTime::RequestedApplyTimes::Immediate;

void SPIDeviceCodeUpdater::continueUpdate(sdbusplus::message::unix_fd image, sdbusplus::common::xyz::openbmc_project::software::ApplyTime::RequestedApplyTimes applyTime, const std::string& oldSwId, const std::string& newswid) {

	std::shared_ptr<Software> newsw = softwares[newswid];

	// design: Create Interface xyz.openbmc_project.Software.Activation at ObjectPath with Status = NotReady
	newsw->activationIntf.activation(actNotReady, false);

	const bool imageVerificationSuccess = verifyImage(image);

	if (!imageVerificationSuccess)
	{
		lg2::error("failed to verify image");
		newsw->activationIntf.activation(actInvalid, false);
		return;
	}

	lg2::info("successfully verified the image");

	newsw->activationIntf.activation(actReady, false);

	// design: Create Interface xyz.openbmc_project.Software.Version at ObjectPath
	// this is already done by our class constructor

	//TODO(design): Create Interface xyz.openbmc_project.Software.ActivationProgress at ObjectPath
	//TODO(design): Create Interface xyz.openbmc_project.Software.ActivationBlocksTransition at ObjectPath

	//design: Activation.Status = Activating
	newsw->activationIntf.activation(actActivating, false);

	// design says we start the update
	writeSPIFlash(image);
	// design says we finished the update here
	newsw->activationIntf.activation(actActive, false);

	//TODO(design):Delete Interface xyz.openbmc_project.Software.ActivationBlocksTransition
	//TODO(design):Delete Interface xyz.openbmc_project.Software.ActivationProgress

	if (applyTime == applyTimeImmediate) {
		//TODO(design): reset device
		//TODO(design): update functional association to system inventory item (instructions unclear)

		//TODO(design): Create Interface xyz.openbmc_project.Software.Update at ObjectPath
		// this is already done by our class constructor, but we should defer that to later
		// since this sw has only been applied at this point in the code

		// design: Delete all interfaces on previous ObjectPath
		// makes sense, we do not want the old sw version to be updated or displayed,
		// since it is not the active version

		deleteOldSw(oldSwId);

		//TODO(design): Create active association to System Inventory Item
	}
}

