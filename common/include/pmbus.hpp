#pragma once

#include <cstdint>

enum class PMBusCmd : uint8_t
{
    page = 0x00,
    writeProtect = 0x10,
    storeUserCode = 0x17,
    mfrId = 0x99,
    mfrSerial = 0x9E,
    icDeviceId = 0xAD,
};
