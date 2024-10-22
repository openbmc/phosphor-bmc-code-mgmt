#pragma once

#include "interfaces/i2c/i2c.hpp"
#include "vr.hpp"

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

#define REG_LOCK_PASSWORD 0x7F48680C

#define XDPE15284C_CONF_SIZE 1344
#define XDPE19283B_CONF_SIZE 1416

#define VR_WARN_REMAINING 3
#define SECT_MAX_CNT 16
#define MAX_SECT_DATA_CNT 200

namespace VR
{

enum revision_code
{
    REV_A = 0x00,
    REV_B,
    REV_C,
    REV_D,
};

enum product_id
{
    PRODUCT_ID_XDPE15284 = 0x8A,
    PRODUCT_ID_XDPE19283 = 0x95,
};

struct config_sect
{
    uint8_t type;
    uint16_t data_cnt;
    uint32_t data[MAX_SECT_DATA_CNT];
};

struct xdpe152xx_config
{
    uint32_t sum_exp;
    uint8_t sect_cnt;
    struct config_sect section[SECT_MAX_CNT];
};

class XDPE152XX : public VR::VoltageRegulator
{
  public:
    explicit XDPE152XX(std::unique_ptr<i2c::I2C> i2cInf) :
        inf(std::move(i2cInf))
    {}

    static std::unique_ptr<VR::VoltageRegulator>
        create(std::unique_ptr<i2c::I2C> inf);

    int get_device_id(uint8_t* dev_id) final;
    int ctl_regs(bool lock) final;
    int mfr_fw_cmd(uint8_t cmd, uint8_t* data, uint8_t* resp) final;
    int get_remaining_writes(uint8_t* remain) final;
    int get_crc(uint32_t* sum) final;

    int program(struct xdpe152xx_config* cfg, bool force);
    int get_config_size(uint8_t devid, uint8_t rev);

  private:
    std::unique_ptr<i2c::I2C> inf;
};

// End VR namespace
} // namespace VR
