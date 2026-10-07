#include "cpld/lattice/lattice_base_cpld.hpp"

#include <gtest/gtest.h>

using namespace phosphor::software::cpld;

TEST(LatticeChipStrTest, TypeString)
{
    EXPECT_EQ(getLatticeChipStr(latticeChip::LCMXO3LF_4300C,
                                latticeStringType::typeString),
              "LatticeLCMXO3LF_4300CFirmware");
}

TEST(LatticeChipStrTest, ModelString)
{
    EXPECT_EQ(getLatticeChipStr(latticeChip::LFMXO5_35T,
                                latticeStringType::modelString),
              "LFMXO5-35T");
}

TEST(LatticeChipStrTest, UnsupportedChipReturnsEmpty)
{
    EXPECT_EQ(getLatticeChipStr(latticeChip::UNSUPPORTED,
                                latticeStringType::typeString),
              "");
    EXPECT_EQ(getLatticeChipStr(latticeChip::UNSUPPORTED,
                                latticeStringType::modelString),
              "");
}

TEST(LatticeChipStrTest, AllSupportedChipsHaveStrings)
{
    for (const auto& [chip, info] : supportedDeviceMap)
    {
        EXPECT_FALSE(
            getLatticeChipStr(chip, latticeStringType::typeString).empty());
        EXPECT_FALSE(
            getLatticeChipStr(chip, latticeStringType::modelString).empty());
    }
}
