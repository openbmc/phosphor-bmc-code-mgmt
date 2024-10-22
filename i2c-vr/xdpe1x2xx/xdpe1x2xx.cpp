#include "xdpe1x2xx.hpp"

#include "common/i2c/i2c.hpp"

#include <cstdio>
#include <unistd.h>
#include <phosphor-logging/lg2.hpp>

int XDPE152XX::getDeviceId(uint8_t* devID)
{
    int ret = -1;
    uint8_t tbuf[16] = {0};
    tbuf[0] = PMBUS_IC_DEVICE_ID;
    tbuf[1] = 2;
    uint8_t rbuf[16] = {0};
    uint8_t rSize = IFX_IC_DEV_ID_LEN + 1;

    ret = this->I2cInterface->sendReceive(tbuf, 1, rbuf, rSize);
    if (ret < 0)
    {
        return ret;
    }

    std::memcpy(devID, &rbuf[1], IFX_IC_DEV_ID_LEN);

    return 0;
}

int XDPE152XX::ctlRegs(bool lock, uint32_t password) {
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
        tBuf[2] = (password >> 24) & 0xFF;
        tBuf[3] = (password >> 16) & 0xFF;
        tBuf[4] = (password >> 8) & 0xFF;
        tBuf[5] = (password >> 0) & 0xFF;
    }

    uint8_t rBuf[16] = {0};
    uint8_t rSize = 4;

    return this->I2cInterface->sendReceive(tBuf, tSize, rBuf, rSize);
}

int XDPE152XX::mfrFWcmd(uint8_t cmd, uint8_t* data, uint8_t* resp)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    if (data)
    {
        tBuf[0] = IFX_MFR_FW_CMD_DATA;
        tBuf[1] = 4; // Block write 4 bytes
        std::memcpy(&tBuf[2], data, 4);
        ret = this->I2cInterface->sendReceive(tBuf, 6, rBuf, 0);
        if ( ret < 0 )
        {
            return -1;
        }
    }

   ::usleep(300);

   tBuf[0] = IFX_MFR_FW_CMD;
   tBuf[1] = cmd;
   ret = this->I2cInterface->sendReceive(tBuf, 2, rBuf, 0);
   if (ret < 0)
   {
        return ret;
   }

   ::usleep(20000);

   if (resp)
   {
        tBuf[0] = IFX_MFR_FW_CMD_DATA;
        ret = this->I2cInterface->sendReceive(tBuf, 1, rBuf, 6);
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

int XDPE152XX::getRemainingWrites(uint8_t* remain)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t devId[2] = {0};

    ret = this->mfrFWcmd(MFR_FW_CMD_RMNG, tBuf, rBuf);
    if (ret <  0)
    {
        return ret;
    }

    ret = this->getDeviceId(devId);
    if (ret < 0)
    {
        return ret;
    }

    int config_size = this->getConfigSize(devId[1], devId[0]);

    *remain = REMAINING_TIMES(rBuf, config_size);

    return 0;
}

int XDPE152XX::getConfigSize(uint8_t devid, uint8_t rev)
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

int XDPE152XX::getCRC(uint32_t* sum)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};

    ret = this->mfrFWcmd(MFR_FW_CMD_GET_CRC, tBuf, rBuf);
    if (ret < 0)
    {
        return ret;
    }

    *sum = (static_cast<uint32_t>(rBuf[0]) << 24) |
           (static_cast<uint32_t>(rBuf[1]) << 16) |
           (static_cast<uint32_t>(rBuf[2]) << 8 ) |
           (static_cast<uint32_t>(rBuf[3]));

    return 0;
}

int XDPE152XX::program(struct xdpe152xxConfig* cfg, bool force)
{
    int ret = -1;
    uint8_t tBuf[16] = {0};
    uint8_t rBuf[16] = {0};
    uint8_t remain = 0;
    uint32_t sum = 0;
    int size = 0;

    if (this->getCRC(&sum) < 0)
    {
        return -1;
    }

    if (!force && ( sum == cfg->sumExp ))
    {
        return -1;
    }

    if (this->getRemainingWrites(&remain) < 0)
    {
        return -1;
    }

    if (!remain)
    {
        return -1;
    }

    if (!force && (remain <= VR_WARN_REMAINING))
    {
        return -1;
    }

    // Added reprogramming of the entire configuration file.
    // Except for the trim section, all other data will be replaced.
    // If the old sections are not invalidated in OTP, they can affect the CRC calculation

    tBuf[0] = 0xfe;
    tBuf[1] = 0xfe;
    tBuf[2] = 0x00;
    tBuf[3] = 0x00;

    ret = this->mfrFWcmd(MFW_FW_CMD_OTP_FILE_INVD, tBuf, NULL);
    if (ret < 0 )
    {
        return ret;
    }

    ::usleep(500000);

    for ( int i = 0; i < cfg->sectCnt; i++)
    {
        struct configSect* sect = &cfg->section[i];
        if (sect == NULL)
        {
            ret = -1;
            break;
        }

        if ((i <=0) || (sect->type != cfg->section[i-1].type))
        {
            tBuf[0] = PMBUS_STL_CML;
            tBuf[1] = 0x1;
            ret = this->I2cInterface->sendReceive(tBuf, 2, rBuf, 0);
            if (ret < 0)
            {
                break;
            }

            tBuf[0] = sect->type;
            tBuf[1] = 0x00;
            tBuf[2] = 0x00;
            tBuf[3] = 0x00;

            ret = this->mfrFWcmd(MFW_FW_CMD_OTP_FILE_INVD, tBuf, NULL);
            if (ret < 0)
            {
                break;
            }

            ::usleep(10000); //Write delay

            tBuf[0] = IFX_MFR_AHB_ADDR;
            tBuf[1] = 4;
            tBuf[2] = 0x00;
            tBuf[3] = 0xe0;
            tBuf[4] = 0x05;
            tBuf[5] = 0x20;

            ret = this->I2cInterface->sendReceive(tBuf, 6, rBuf, 0);
            if (ret < 0)
            {
                break;
            }

            ::usleep(10000);
            size = 0;
        }

        // programm into scratchpad
        for(int j = 0; j< sect->dataCnt;j++)
        {
            tBuf[0] = IFX_MFR_REG_WRITE;
            tBuf[1] = 4;
            memcpy(&tBuf[2], &sect->data[j], 4);
            ret = this->I2cInterface->sendReceive(tBuf, 6, rBuf, 0);
            if (ret < 0)
            {
                break;
            }
            ::usleep(10000);
        }
        if (ret)
        {
            break;
        }

        size += sect->dataCnt * 4;
        if ((i+1 >= cfg->sectCnt) || (sect->type != cfg->section[i+1].type))
        {
            //Upload to scratchpad
            std::memcpy(tBuf, &size, 2);
            tBuf[2] = 0x00;
            tBuf[3] = 0x00;
            ret = this->mfrFWcmd(MFR_FW_CMD_OTP_CONF_STO, tBuf, NULL);
            if (ret < 0)
            {
                break;
            }

            // wait for programming soak (2ms/byte, at least 200ms)
            // ex: Config (604 bytes): (604 / 50) + 2 = 14 (1400 ms)
            size = (size / 50) + 2;
            for (int j = 0; j < size; j++)
            {
                ::usleep(100000);
            }

            tBuf[0] = PMBUS_STL_CML;
            ret = this->I2cInterface->sendReceive(rBuf, 1, tBuf, 1);
            if (ret < 0)
            {
                break;
            }
            if (rBuf[0] & 0x01)
            {
                ret = -1;
                break;
            }
        }
    }

    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

int XDPE152XX::parseImage(struct xdpe152xxConfig* cfg, const uint8_t* image, size_t image_size)
{
    (void)*cfg;
    (void)image;
    (void)image_size;
    return 0;
}
