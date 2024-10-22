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

} // namespace VR
