#pragma once

#include <cstdint>

enum class PMBusCmd : uint8_t
{
    Page = 0x00,
    WriteProtect = 0x10,
    StoreUserCode = 0x17,
    MFRId = 0x99,
    MFRSerial = 0x9E,
    IcDeviceId = 0xAD,
};
