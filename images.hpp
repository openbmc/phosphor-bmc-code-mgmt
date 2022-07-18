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
            const std::vector<std::string> bmcImages = {"image-kernel", 
                                "image-rofs", "image-rwfs", "image-section"};
            // BMC flash image file name list for full flash image (image-bmc)
            const std::string bmcFullImages = {"image-bmc"};

            std::vector<std::string> getOptionalImages();

        } // namespace image
    } // namespace software
} // namespace phosphor
