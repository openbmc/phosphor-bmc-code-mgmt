#include "config.h"

#include "flash.hpp"

#include "activation.hpp"
#include "images.hpp"

#include <experimental/filesystem>
#include <fstream>

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

namespace fs = std::experimental::filesystem;

void Activation::flashWrite()
{
    // For static layout code update, just put images in /run/initramfs.
    // It expects user to trigger a reboot and an updater script will program
    // the image to flash during reboot.
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);
    for (auto& bmcFlashImage : phosphor::software::image::bmcFlashImages)
    {
        fs::path file(uploadDir.c_str());
        file /= versionId;
        file /= bmcFlashImage;
        std::ifstream efile(file.c_str());
        if (efile.good() != 1)
        {
            for (auto& bmcImage : phosphor::software::image::bmcImages)
            {
                fs::copy_file(uploadDir / versionId / bmcImage, toPath / bmcImage,
                              fs::copy_options::overwrite_existing);
            }
        }
        else
        {
            fs::copy_file(uploadDir / versionId / bmcFlashImage, toPath / bmcFlashImage,
                          fs::copy_options::overwrite_existing);
        }
    }
}

void Activation::onStateChanges(sdbusplus::message::message& /*msg*/)
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
