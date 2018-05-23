#include <experimental/filesystem>

#include "activation.hpp"
#include "config.h"
#include "flash.hpp"
#include "image_verify.hpp" // For bmcImages

namespace
{
constexpr auto PATH_INITRAMFS = "/run/initramfs";
} // anonymous

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

void Activation::flashWrite()
{
    // For non-ubifs code update, putting image in /run/initramfs
    // and reboot, an updater script will program the image to flash
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);
    for (auto& bmcImage : phosphor::software::image::bmcImages)
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
} // namepsace phosphor
