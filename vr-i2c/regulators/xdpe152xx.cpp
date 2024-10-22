#include "xdpe152xx.hpp"

#include "interfaces/i2c/i2c.hpp"

#include <cstdio>
#include <phosphor-logging/lg2.hpp>

namespace VR
{

std::unique_ptr<VR::VoltageRegulator>
    XDPE152XX::create(std::unique_ptr<i2c::I2C> inf)
{
    std::unique_ptr<XDPE152XX> dev =
        std::make_unique<XDPE152XX>(std::move(inf));

    return dev;
}

int XDPE152XX::get_device_id(uint8_t* devID)
{
    uint8_t tbuf[16] = {0};
    tbuf[0] = PMBUS_IC_DEVICE_ID;
    tbuf[1] = 2;
    uint8_t rbuf[16] = {0};
    uint8_t rSize = IFX_IC_DEV_ID_LEN + 1;

    try
    {
        this->inf->send_receive(tbuf, 1, rbuf, rSize);
    }
    catch (i2c::I2CException& e)
    {
        lg2::error("getDeviceID failed: {ERR}", "ERR", e.what());
        return -1;
    }

    std::memcpy(devID, &rbuf[2], IFX_IC_DEV_ID_LEN);

    return 0;
}

int XDPE152XX::ctl_regs(bool lock) {
    uint8_t tBuf[16] = {0};
    tBuf[0] = MFR_DISABLE_SECURITY_ONCE;
    tBuf[1] = 4;
    uint8_t tSize = 6;

    if (lock)
    {
        tBuf[2] = 0x1;
    }
    else
    {
        tBuf[2] = (REG_LOCK_PASSWORD >> 24) & 0xFF;
        tBuf[3] = (REG_LOCK_PASSWORD >> 16) & 0xFF;
        tBuf[4] = (REG_LOCK_PASSWORD >> 8) & 0xFF;
        tBuf[5] = (REG_LOCK_PASSWORD >> 0) & 0xFF;
    }

    uint8_t rBuf[16] = {0};
    uint8_t rSize = 0;

    try
    {
        this->inf->send_receive(tBuf, tSize, rBuf, rSize);
    }
    catch (i2c::I2CException& e)
    {
        lg2::error("ctl_regs() failed: {ERR}", "ERR", e.what());
        return -1;
    }

    return 0;
}


} // namespace VR
