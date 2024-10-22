#pragma once

#include "common/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <cstdint>

#define REMAINING_TIMES(x, y) (((x[1] << 8) | x[0]) / (y))

#define PMBUS_IC_DEVICE_ID 0xAD
#define PMBUS_STL_CML 0x7E

#define IFX_IC_DEV_ID_LEN 2

#define IFX_MFR_AHB_ADDR 0xCE
#define IFX_MFR_REG_WRITE 0xDE
#define IFX_MFR_FW_CMD_DATA 0xFD
#define IFX_MFR_FW_CMD 0xFE

#define MFR_DISABLE_SECURITY_ONCE 0xCB

#define MFR_FW_CMD_RMNG 0x10
#define MFR_FW_CMD_OTP_CONF_STO 0x11
#define MFW_FW_CMD_OTP_FILE_INVD 0x12
#define MFR_FW_CMD_GET_CRC 0x2D

#define XDPE15284C_CONF_SIZE 1344
#define XDPE19283B_CONF_SIZE 1416

#define VR_WARN_REMAINING 3
#define SECT_MAX_CNT 16
#define MAX_SECT_DATA_CNT 200


enum revisionCode
{
    REV_A = 0x00,
    REV_B,
    REV_C,
    REV_D,
};

enum productId
{
    PRODUCT_ID_XDPE15284 = 0x8A,
    PRODUCT_ID_XDPE19283 = 0x95,
};

struct configSect
{
    uint8_t type;
    uint16_t dataCnt;
    uint32_t data[MAX_SECT_DATA_CNT];
};

struct xdpe152xxConfig
{
    uint32_t sumExp;
    uint8_t sectCnt;
    struct configSect section[SECT_MAX_CNT];
};

class XDPE152XX : public VoltageRegulator
{
  public:
    explicit XDPE152XX(std::unique_ptr<phosphor::i2c::I2C> i2cInf) :
        I2cInterface(std::move(i2cInf))
    {}

    static std::unique_ptr<VoltageRegulator>
        create(std::unique_ptr<phosphor::i2c::I2C> inf);

    int getDeviceId(uint8_t* dev_id) final;
    int ctlRegs(bool lock, uint32_t password) final;
    int mfrFWcmd(uint8_t cmd, uint8_t* data, uint8_t* resp) final;
    int getRemainingWrites(uint8_t* remain) final;
    int getCRC(uint32_t* sum) final;

    int parseImage(struct xdpe152xx_config *cfg, const uint8_t* image, size_t image_size);
    int program(struct xdpe152xxConfig* cfg, bool force);
    int getConfigSize(uint8_t devid, uint8_t rev);

  private:
    std::unique_ptr<phosphor::i2c::I2C> I2cInterface;
};

