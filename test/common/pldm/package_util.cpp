#include "common/pldm/pldm_package_util.hpp"
#include "test/create_package/create_pldm_fw_package.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using phosphor::software::device::ComponentImage;

constexpr uint32_t testVendorIANA = 0x0000a015;
constexpr const char* testCompatible = "com.example.Hardware.Test";

class ExtractMatchingComponentImagesTest : public testing::Test
{
  protected:
    // Parses a package over 'buffer' and extracts the images for the device
    bool extract(const std::vector<TestComponentImage>& components,
                 std::vector<ComponentImage>& imagesOut)
    {
        size_t size = 0;
        buffer = create_pldm_package_buffer(components, testVendorIANA,
                                            testCompatible, size);
        auto package = pldm_package_util::parsePLDMPackage(buffer.get(), size);
        if (package == nullptr)
        {
            return false;
        }
        return pldm_package_util::extractMatchingComponentImages(
            package, testCompatible, testVendorIANA, imagesOut);
    }

    std::unique_ptr<uint8_t[]> buffer;
};

static void expectImage(const ComponentImage& image,
                        const TestComponentImage& expected)
{
    EXPECT_TRUE(std::ranges::equal(image.image, expected.data));
    EXPECT_EQ(image.version, expected.version);
}

TEST_F(ExtractMatchingComponentImagesTest, ReturnsAllInPackageOrder)
{
    const std::vector<TestComponentImage> components = {
        {{0x11, 0x12, 0x13}, "version-a"},
        {{0x21, 0x22}, "version-b"},
        {{0x31, 0x32, 0x33, 0x34}, "version-c"},
    };

    std::vector<ComponentImage> images;
    ASSERT_TRUE(extract(components, images));

    ASSERT_EQ(images.size(), components.size());
    for (size_t c = 0; c < components.size(); c++)
    {
        expectImage(images[c], components[c]);
    }
}

TEST_F(ExtractMatchingComponentImagesTest, SkipsComponentsNotApplicable)
{
    const std::vector<TestComponentImage> components = {
        {{0x11, 0x12, 0x13}, "version-a"},
        {{0x21, 0x22}, "version-b", false},
        {{0x31, 0x32, 0x33, 0x34}, "version-c"},
    };

    std::vector<ComponentImage> images;
    ASSERT_TRUE(extract(components, images));

    ASSERT_EQ(images.size(), 2);
    expectImage(images[0], components[0]);
    expectImage(images[1], components[2]);
}
