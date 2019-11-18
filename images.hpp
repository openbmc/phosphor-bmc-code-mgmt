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
// BMC flash image file name list for full flash image (image-bmc)
const std::string bmcFullImages = {"image-bmc"};

} // namespace image
} // namespace software
} // namespace phosphor
