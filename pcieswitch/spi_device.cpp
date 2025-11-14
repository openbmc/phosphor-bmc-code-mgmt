#include "spi_device.hpp"

#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"
#include "common/include/utils.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstddef>
#include <fstream>
#include <random>

PHOSPHOR_LOG2_USING;

using namespace std::literals;
using namespace phosphor::software;
using namespace phosphor::software::manager;

void SPIDevice::getFt4222AdapterNum(bool* devExist, std::string& adapterNum)
{
    if ((chipName == "Anacapa_LBB_PCIESW") && devExist[LBB])
    {
        if (devExist[RBB] && devExist[AMC])
        {
            adapterNum = "4";
        }
        else if (devExist[RBB] && !devExist[AMC])
        {
            adapterNum = "2";
        }
        else if (!devExist[RBB] && !devExist[AMC])
        {
            adapterNum = "0";
        }
    }

    if ((chipName == "Anacapa_RBB_PCIESW") && devExist[RBB])
    {
       if (devExist[LBB] && devExist[AMC])
       {
           adapterNum = "2";
       }
       else if (devExist[LBB] && !devExist[AMC])
       {
           adapterNum = "0";
       }
       else if (!devExist[LBB] && !devExist[AMC])
       {
           adapterNum = "0";
       }
    }
}
                                                     
sdbusplus::async::task<bool> SPIDevice::updateDevice(const uint8_t* image,
                                                     size_t imageSize)
{ 
    std::string linuxCmd;
    bool success;
    bool devExist[FT4222_END] = {false};
    std::vector<std::string> devSysPath = {lbbPCIeSwitchSysPath, 
                                rbbPCIeSwitchSysPath, amcPCIeSwitchSysPath};
    using phosphor::software::Software;
    const std::string path = "/tmp/pcieswitch-image-" +
                             std::to_string(Software::getRandomId()) + ".bin";

    int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        lg2::info("Failed to open file: {PATH}", "PATH", path);
        co_return false;
    }

    const ssize_t bytesWritten = write(fd, image, imageSize);
    if (bytesWritten < 0)
    {
        lg2::error("Failed to write image to file");
        close(fd);
        co_return false;
    }

    close(fd);

    std::string adapterNum;    
    for (size_t i = 0; i < devSysPath.size(); i++)
    {
        if (std::filesystem::exists(devSysPath[i]))
        {
            std::ifstream file(devSysPath[i]);
            if (!file.is_open())
            {
                lg2::error("fail to open {SYS}", "SYS", devSysPath[i]);
                co_return false;
            }

            std::string deviceId;
            std::getline(file, deviceId);
            file.close();

            deviceId.erase(0, deviceId.find_first_not_of(" \t\n\r"));
            deviceId.erase(deviceId.find_last_not_of(" \t\n\r") + 1);

            if (deviceId != ft4222DID)
            {
                lg2::error("the usb device is not FT4222");
                co_return false;
            }

            devExist[i] = true;
        }
    }

    getFt4222AdapterNum(devExist, adapterNum);

    setUpdateProgress(10);

    std::string tclPath = "/etc/update_pex_spi.tcl";

    std::ifstream input(tclPath);
    if (!input.is_open())
    {
        lg2::error("could not open update_pex_spi.tcl");
        co_return false;
    }

    std::ostringstream buffer;
    std::string line;

    while (std::getline(input, line))
    {
        if (line.find("adapter_open") != std::string::npos)
        {
            line = "adapter_open " + adapterNum;
        }
        else if (line.find("set binfile") != std::string::npos)
        {
            line = "set binfile \"" + path + "\"";
        }

        buffer << line << "\n";
    }

    input.close();

    std::ofstream output(tclPath, std::ios::trunc);
    if (!output.is_open())
    {
        lg2::error("ERR: cannot write modify update_pex_spi.tcl");
        co_return false;
    }

    output << buffer.str();
    output.close();

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        linuxCmd = "gpioset $(gpiofind " + gpioLines[i] + ")=" \
                  + gpioValues[i];
        success = co_await asyncSystem(ctx, linuxCmd);
        if (!success)
        {
            lg2::error("ERR: gpioset $(gpiofind {LINE})={VAL}",
                "LINE", gpioLines[i], "VAL", gpioValues[i]);
        }
    }

    co_await sdbusplus::async::sleep_for(
            ctx, std::chrono::seconds(1));

    linuxCmd = "usbio -f /etc/update_pex_spi.tcl";
    lg2::info("execute {CMD}", "CMD", linuxCmd);
    success = co_await asyncSystem(ctx, linuxCmd);
    if (!success)
    {
        lg2::error("ERR: {CMD}", "CMD", linuxCmd);
        co_return success;
    }

    for (size_t i = 0; i < gpioLines.size(); i++)
    {
        std::string inverted = (gpioValues[i] == "1") ? "0" : "1";
        linuxCmd = "gpioset $(gpiofind " + gpioLines[i] + ")=" \
                  + inverted;
        success = co_await asyncSystem(ctx, linuxCmd);
        if (!success)
        {
            lg2::error("ERR: gpioset $(gpiofind {LINE})={VAL}",
                "LINE", gpioLines[i], "VAL", inverted);
            co_return success;
        }
    }

    std::filesystem::remove(path);

    setUpdateProgress(100);

    co_return success;
}

sdbusplus::async::task<bool> SPIDevice::getVersion(std::string& version)
{
    // wait the OOB solution from Broadcom
    version = "";

    co_return true;
}

