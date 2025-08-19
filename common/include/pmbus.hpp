#pragma once

#include <cstdint>

enum class PMBusCmd : uint8_t
{
    page = 0x00,
    writeProtect = 0x10,
    restoreUserAll = 0x16,
    storeUserCode = 0x17,
    statusCML = 0x7E,
    mfrId = 0x99,
    mfrModel = 0x9A,
    mfrSerial = 0x9E,
    icDeviceId = 0xAD,
};
