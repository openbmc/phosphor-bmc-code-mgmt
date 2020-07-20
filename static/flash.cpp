#include "config.h"

#include "flash.hpp"

#include "activation.hpp"
#include "images.hpp"
#include "item_updater.hpp"

#include <filesystem>

namespace
{
constexpr auto PATH_INITRAMFS = "/run/initramfs";
} // namespace

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::filesystem;
using namespace phosphor::software::image;

void Activation::flashWrite()
{
    // For static layout code update, just put images in /run/initramfs.
    // It expects user to trigger a reboot and an updater script will program
    // the image to flash during reboot.
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);

    for (const auto& bmcImage : parent.imageUpdateList)
    {
        fs::copy_file(uploadDir / versionId / bmcImage, toPath / bmcImage,
                      fs::copy_options::overwrite_existing);
    }
}

void Activation::onStateChanges(sdbusplus::message::message& /*msg*/)
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
