#include "usb_rcm_driver.hpp"

#include <CLI/CLI.hpp>

#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace phosphor::software::usb_rcm;

int main(int argc, char** argv)
{
    CLI::App app{
        "NVIDIA USB RCM (Vera CPU) firmware recovery bring-up tool. Operates on "
        "a single device described by the options below."};
    app.require_subcommand(1);

    std::string usbPort;
    std::string name = "CPU_0";
    std::string configTypeStr = "c2";
    std::string dotBlobDir = "/var/emmc/misc/dot-blob";
    std::vector<std::string> imageFiles;
    bool verbose = false;
    int recoveryInterface = 3;
    std::string recoveryEndpointStr; // empty = auto-discover from descriptor

    app.add_option("--usb-port", usbPort,
                   "USB port path of the RCM device, e.g. 1-9");
    app.add_option("--name", name,
                   "Device name; its trailing index selects the DOT blob");
    app.add_option("--config-type", configTypeStr,
                   "Force-recovery GPIO layout: c2, c1g2 or c2g4")
        ->check(CLI::IsMember({"c2", "c1g2", "c2g4"}));
    app.add_option("--dot-blob-dir", dotBlobDir,
                   "Directory holding the per-CPU DOT blobs");
    app.add_option("--recovery-interface", recoveryInterface,
                   "USB interface number of the RCM recovery interface "
                   "(default 3)");
    app.add_option("--recovery-endpoint", recoveryEndpointStr,
                   "Bulk-OUT endpoint on the recovery interface, e.g. 0x0a "
                   "(default: auto-discover from the interface descriptor)");
    app.add_flag("-v,--verbose", verbose, "Verbose output");

    CLI::App* statusCmd = app.add_subcommand(
        "GetRecoveryStatus", "Probe and print the device recovery state");
    CLI::App* performCmd = app.add_subcommand(
        "PerformUSBRecovery",
        "Stream firmware component images to the device over RCM");
    performCmd
        ->add_option("-i,--images", imageFiles,
                     "Component image files, in RCM flash order")
        ->required();
    CLI::App* forceCmd = app.add_subcommand(
        "SetForceRecovery", "Drive the device into recovery mode via GPIO");
    CLI::App* clearCmd = app.add_subcommand(
        "ClearForceRecovery", "Restore the default cold-boot strap states");

    CLI11_PARSE(app, argc, argv);

    DeviceConfig cfg;
    cfg.name = name;
    cfg.usbPort = usbPort;
    cfg.dotBlobDir = dotBlobDir;
    cfg.recoveryInterface = recoveryInterface;
    if (!recoveryEndpointStr.empty())
    {
        try
        {
            cfg.recoveryEndpoint = static_cast<uint8_t>(
                std::stoul(recoveryEndpointStr, nullptr, 0));
        }
        catch (...)
        {
            std::cerr << "Invalid --recovery-endpoint: " << recoveryEndpointStr
                      << "\n";
            return 1;
        }
    }
    if (auto configType = parseConfigType(configTypeStr))
    {
        cfg.configType = *configType;
    }

    RecoveryDriver driver(std::move(cfg));

    if (*statusCmd)
    {
        RecoveryState state = driver.status();
        std::cout << name << ": " << toString(state) << "\n";
        return (state == RecoveryState::unreachable) ? 1 : 0;
    }

    if (*forceCmd)
    {
        if (!driver.forceRecovery())
        {
            std::cerr << "Force recovery failed: " << driver.lastError()
                      << "\n";
            return 1;
        }
        std::cout << name << ": force-recovery straps asserted\n";
        return 0;
    }

    if (*clearCmd)
    {
        if (!driver.clearForceRecovery())
        {
            std::cerr << "Clear force recovery failed: " << driver.lastError()
                      << "\n";
            return 1;
        }
        std::cout << name << ": default strap states restored\n";
        return 0;
    }

    if (*performCmd)
    {
        // Read the image files into memory, preserving the given order (which
        // is the RCM flash order). buffers is reserved so its element data
        // pointers stay valid while images references them.
        std::vector<std::vector<uint8_t>> buffers;
        std::vector<Image> images;
        buffers.reserve(imageFiles.size());
        for (const std::string& path : imageFiles)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "Cannot open image " << path << "\n";
                return 1;
            }
            buffers.emplace_back(std::istreambuf_iterator<char>(file),
                                 std::istreambuf_iterator<char>());
            images.push_back(
                {buffers.back().data(), buffers.back().size(), path});
        }

        RecoveryResult result = driver.recover(images, verbose);
        if (result.success)
        {
            std::cout << name << ": recovery "
                      << (result.emptyDotAccepted ? "accepted (empty DOT blob)"
                                                  : "successful")
                      << "\n";
            return 0;
        }
        std::cerr << name << ": recovery failed: " << result.error
                  << " (code 0x" << std::hex << result.errorCode << ")\n";
        return 1;
    }

    return 0;
}
