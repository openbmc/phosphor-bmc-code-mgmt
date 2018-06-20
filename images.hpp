#include <string>
#include <vector>

#include "config.h"


namespace phosphor
{
namespace software
{
namespace image
{

// BMC flash image file name list.
#ifdef UBIFS_LAYOUT
const std::vector<std::string> bmcImages = {"image-kernel", "image-rofs",
                                            "image-rwfs", "image-u-boot"};
#else
const std::vector<std::string> bmcImages = {"image-bmc"};
#endif

} // namespace image
} // namespace software
} // namespace phosphor
