#include "config.h"

#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace image
{

// BMC flash image file name list.
const std::vector<std::string> bmcImages = {"image-kernel", "image-rofs",
                                            "image-rwfs", "image-u-boot"};

// Kernel image file name
constexpr auto kernelImage = "image-kernel";

} // namespace image
} // namespace software
} // namespace phosphor
