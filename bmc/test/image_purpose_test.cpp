#include "item_updater.hpp"

#include <gtest/gtest.h>

namespace phosphor::software::updater
{
namespace
{

using Purpose = VersionClass::VersionPurpose;
using Type = ItemUpdater::UpdaterType;

struct ImagePurposeCase
{
    const char* name;
    Purpose purpose;
    Type type;
    bool expected;
};

class ImagePurposeTest : public testing::TestWithParam<ImagePurposeCase>
{};

TEST_P(ImagePurposeTest, MatchesUpdaterTarget)
{
    const auto& testCase = GetParam();
    EXPECT_EQ(ItemUpdater::verifyImagePurpose(testCase.purpose, testCase.type),
              testCase.expected);
}

INSTANTIATE_TEST_SUITE_P(
    UpdaterTargets, ImagePurposeTest,
    testing::Values(
        ImagePurposeCase{"BMCOnBMC", Purpose::BMC, Type::BMC, true},
        ImagePurposeCase{"BMCOnBIOS", Purpose::BMC, Type::BIOS, false},
        ImagePurposeCase{"BMCOnAll", Purpose::BMC, Type::ALL, true},
        ImagePurposeCase{"SystemOnBMC", Purpose::System, Type::BMC, true},
        ImagePurposeCase{"SystemOnBIOS", Purpose::System, Type::BIOS, false},
        ImagePurposeCase{"SystemOnAll", Purpose::System, Type::ALL, true},
        ImagePurposeCase{"HostOnBMC", Purpose::Host, Type::BMC, false},
        ImagePurposeCase{"HostOnBIOS", Purpose::Host, Type::BIOS, true},
        ImagePurposeCase{"HostOnAll", Purpose::Host, Type::ALL, true},
        ImagePurposeCase{"UnknownOnBMC", Purpose::Unknown, Type::BMC, true},
        ImagePurposeCase{"UnknownOnBIOS", Purpose::Unknown, Type::BIOS, true},
        ImagePurposeCase{"UnknownOnAll", Purpose::Unknown, Type::ALL, true}),
    [](const testing::TestParamInfo<ImagePurposeCase>& info) {
        return info.param.name;
    });

} // namespace
} // namespace phosphor::software::updater
