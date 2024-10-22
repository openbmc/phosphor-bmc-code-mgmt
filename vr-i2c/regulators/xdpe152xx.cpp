#include "xdpe152xx.hpp"

#include "interfaces/i2c/i2c.hpp"

#include <cstdio>
#include <unistd.h>
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
    int ret = -1;
    uint8_t tbuf[16] = {0};
    tbuf[0] = PMBUS_IC_DEVICE_ID;
    tbuf[1] = 2;
    uint8_t rbuf[16] = {0};
    uint8_t rSize = IFX_IC_DEV_ID_LEN + 1;

    ret = this->inf->send_receive(tbuf, 1, rbuf, rSize);
    if (ret < 0)
    {
        return ret;
    }

    //rbuf[1] == DevID, rbuf[2]
    std::memcpy(devID, &rbuf[1], IFX_IC_DEV_ID_LEN);

    return 0;
}

int XDPE152XX::ctl_regs(bool lock) {
    int ret = -1;
    uint8_t tBuf[16] = {0};
    tBuf[0] = MFR_DISABLE_SECURITY_ONCE;
    tBuf[1] = 4;
    uint8_t tSize = 6;

    if (lock)
    {
        tBuf[2] = 0x1; // lock
    }
    else
    {
        // unlock with password
        tBuf[2] = (REG_LOCK_PASSWORD >> 24) & 0xFF;
        tBuf[3] = (REG_LOCK_PASSWORD >> 16) & 0xFF;
        tBuf[4] = (REG_LOCK_PASSWORD >> 8) & 0xFF;
        tBuf[5] = (REG_LOCK_PASSWORD >> 0) & 0xFF;
    }

    uint8_t rBuf[16] = {0};
    uint8_t rSize = 4;

    ret = this->inf->send_receive(tBuf, tSize, rBuf, rSize);
    if (ret < 0)
    {
        return ret;
    }

    return ret;
}

int XDPE152XX::mfr_fw_cmd(uint8_t cmd, uint8_t* data, uint8_t* resp)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    if (data)
    {
        tBuf[0] = IFX_MFR_FW_CMD_DATA;
        tBuf[1] = 4; // Block write 4 bytes
        std::memcpy(&tBuf[2], data, 4);
        ret = this->inf->send_receive(tBuf, 6, rBuf, 0);
        if ( ret < 0 )
        {
            return -1;
        }
    }

   ::usleep(300);

   tBuf[0] = IFX_MFR_FW_CMD;
   tBuf[1] = cmd;
   ret = this->inf->send_receive(tBuf, 2, rBuf, 0);
   if (ret < 0)
   {
        return ret;
   }

   ::usleep(20000);

   if (resp)
   {
        tBuf[0] = IFX_MFR_FW_CMD_DATA;
        ret = this->inf->send_receive(tBuf, 1, rBuf, 6);
        if (ret < 0)
        {
            return -1;
        }
        if (rBuf[0] != 4)
        {
            return -1;
        }
        std::memcpy(resp, rBuf+1, 4);
   }

    return 0;
}

int XDPE152XX::get_remaining_writes(uint8_t* remain)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t devId[2] = {0};

    ret = this->mfr_fw_cmd(MFR_FW_CMD_RMNG, tBuf, rBuf);
    if (ret <  0)
    {
        return ret;
    }

    ret = this->get_device_id(devId);
    if (ret < 0)
    {
        return ret;
    }

    int config_size = this->get_config_size(devId[1], devId[0]);

    *remain = REMAINING_TIMES(rBuf, config_size);

    return 0;
}

int XDPE152XX::get_config_size(uint8_t devid, uint8_t rev)
{
    int size = XDPE15284C_CONF_SIZE;

    switch (devid)
    {
        case PRODUCT_ID_XDPE19283:
            if (rev == REV_B) {
                size = XDPE19283B_CONF_SIZE;
            }
            break;
    }

  return size;
}

int XDPE152XX::get_crc(uint32_t* sum)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};

    ret = this->mfr_fw_cmd(MFR_FW_CMD_GET_CRC, tBuf, rBuf);
    if (ret < 0)
    {
        return ret;
    }

    std::memcpy(sum, rBuf, sizeof(uint32_t));

    return 0;
}

} // namespace VR
