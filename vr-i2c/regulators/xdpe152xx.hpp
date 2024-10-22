#pragma once

#include "interfaces/i2c/i2c.hpp"
#include "vr.hpp"

#include <cstdint>

#define REMAINING_TIMES(x, y) (((x[1] << 8) | x[0]) / (y))

#define PMBUS_IC_DEVICE_ID 0xAD

#define IFX_IC_DEV_ID_LEN 2
#define IFX_MFR_FW_CMD_DATA 0xFD
#define IFX_MFR_FW_CMD 0xFE

#define MFR_DISABLE_SECURITY_ONCE 0xCB

#define MFR_FW_CMD_RMNG 0x10
#define MFR_FW_CMD_GET_CRC 0x20

#define REG_LOCK_PASSWORD 0x7F48680C

#define XDPE15284C_CONF_SIZE 1344
#define XDPE19283B_CONF_SIZE 1416

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

    int get_config_size(uint8_t devid, uint8_t rev);

  private:
    std::unique_ptr<i2c::I2C> inf;
};

// End VR namespace
} // namespace VR
