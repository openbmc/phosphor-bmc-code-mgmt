#include "usb_rcm_device.hpp"

#include "usb_rcm_driver.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

PHOSPHOR_LOG2_USING;

using namespace std::chrono_literals;

namespace phosphor::software::usb_rcm::device
{

sdbusplus::async::task<bool> USBRCMRecoveryDevice::updateDevice(
    const uint8_t* image, size_t image_size)
{
    const std::vector<DeviceInf::ComponentImage> components = {
        {std::span<const uint8_t>(image, image_size), ""}};
    co_return co_await updateDeviceComponents(components);
}

sdbusplus::async::task<bool> USBRCMRecoveryDevice::updateDeviceComponents(
    const std::vector<DeviceInf::ComponentImage>& components)
{
    if (components.empty())
    {
        error("USB RCM {NAME}: update package has no applicable components",
              "NAME", driverConfig.name);
        co_return false;
    }

    setUpdateProgress(5);

    // The recovery flow blocks (GPIO settle, re-enumeration, and streaming the
    // images can take minutes); run it on a worker thread and keep the reactor
    // responsive by polling. State is shared via a shared_ptr and the worker is
    // detached, so a cancelled coroutine neither touches a destroyed frame nor
    // leaves a joinable thread behind. The component images are copied into
    // owned buffers so the worker never dereferences the (possibly-unmapped)
    // package memory.
    struct SharedState
    {
        std::atomic<bool> done{false};
        std::atomic<bool> success{false};
        std::string error;
        std::vector<std::vector<uint8_t>> imageBuffers;
        DeviceConfig cfg;
    };
    auto state = std::make_shared<SharedState>();
    state->cfg = driverConfig;
    state->imageBuffers.reserve(components.size());
    for (const DeviceInf::ComponentImage& component : components)
    {
        state->imageBuffers.emplace_back(component.image.begin(),
                                         component.image.end());
    }

    std::thread([state]() {
        RecoveryDriver driver(state->cfg);

        // Only recover a device that is already in RCM recovery mode; do not
        // drive force-recovery here.
        RecoveryState recoveryState = driver.status();
        if (recoveryState != RecoveryState::inRecovery &&
            recoveryState != RecoveryState::recoveryComplete)
        {
            state->error =
                std::string("recovery did not progress: device is not in RCM "
                            "recovery mode (status=") +
                toString(recoveryState) + ")";
            state->done.store(true);
            return;
        }

        std::vector<Image> images;
        images.reserve(state->imageBuffers.size());
        for (std::vector<uint8_t>& buffer : state->imageBuffers)
        {
            images.push_back({buffer.data(), buffer.size(), ""});
        }

        RecoveryResult result = driver.recover(images);
        driver.clearForceRecovery();

        state->error = result.error;
        state->success.store(result.success);
        state->done.store(true);
    }).detach();

    uint8_t progress = 5;
    while (!state->done.load())
    {
        co_await sdbusplus::async::sleep_for(ctx, 2s);
        if (progress < 90)
        {
            progress = static_cast<uint8_t>(progress + 5);
            setUpdateProgress(progress);
        }
    }

    if (state->success.load())
    {
        info("USB RCM {NAME}: recovery successful", "NAME", driverConfig.name);
        setUpdateProgress(100);
        co_return true;
    }

    error("USB RCM {NAME}: recovery failed: {ERR}", "NAME", driverConfig.name,
          "ERR", state->error);
    co_return false;
}

} // namespace phosphor::software::usb_rcm::device
