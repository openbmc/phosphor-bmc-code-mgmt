#pragma once

#include <cstdint>

namespace phosphor::software::VR
{

// Columns of an Automated Test Equipment (ATE) format configuration file
enum class ATE : uint8_t
{
    configId = 0,
    pageNum,
    regAddrHex,
    regAddrDec,
    regName,
    regDataHex,
    regDataDec,
    writeType,
    colCount,
};

enum class MPSPage : uint8_t
{
    page0 = 0,
    page1,
    page2,
    page3,
    page4,
};

} // namespace phosphor::software::VR
