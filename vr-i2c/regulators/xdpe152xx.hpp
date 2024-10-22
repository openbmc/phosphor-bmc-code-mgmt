#pragma once

#include "interfaces/i2c/i2c.hpp"
#include "vr.hpp"

#include <cstdint>

#define PMBUS_IC_DEVICE_ID 0xAD

#define IFX_IC_DEV_ID_LEN 3

#define MFR_DISABLE_SECURITY_ONCE 0xCB

#define REG_LOCK_PASSWORD 0x7F48680C


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

  private:
    std::unique_ptr<i2c::I2C> inf;
};

// End VR namespace
} // namespace VR
