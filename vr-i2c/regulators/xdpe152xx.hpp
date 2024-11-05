#pragma once

#include "interfaces/i2c/i2c.hpp"
#include "vr.hpp"

#include <cstdint>

#define ADDRESS_FIELD "PMBus Address :"
#define CHECKSUM_FIELD "Checksum :"
#define DATA_START_TAG "[Configuration Data]"
#define DATA_END_TAG "[End Configuration Data]"
#define DATA_COMMENT "//"
#define DATA_XV "XV"

namespace VR
{

const int MAX_SEC_DATA = 200;
const int MAX_SECT_NUM = 16;
const int IFX_IC_DEV_ID_LEN = 2;

enum
{
    PMBUS_STS_CML = 0x7E,
    PMBUS_IC_DEVICE_ID = 0xAD,
    IFX_MFR_DISABLE_SECURITY_ONCE = 0xCB,
    IFX_MFR_AHB_ADDR = 0xCE,
    IFX_MFR_REG_WRITE = 0xDE,
    IFX_MFR_REG_READ = 0xDF,
    IFX_MFR_FW_CMD_DATA = 0xFD,
    IFX_MFR_FW_CMD = 0xFE,

    FW_RESET = 0x0e,
    OTP_PTN_RMNG = 0x10,
    OTP_CONF_STO = 0x11,
    OTP_FILE_INVD = 0x12,
    GET_CRC = 0x2D,
    XDPE15284C_CONF_SIZE = 1344,
    XDPE19283B_CONF_SIZE = 1416,
    CRC_POLY = 0xEDB88320, // polynomial
    VR_WRITE_DELAY = 1000,
    VR_RELOAD_DELAY = 50000,
};

const uint32_t REG_LOCK_PASSWORD = 0x7F48680C;

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
    uint32_t data[MAX_SEC_DATA];
};

class xdpe152XX_config : public VR::Config
{
  public:
    uint8_t get_address() final;
    uint8_t get_sect_cnt() final;
    uint16_t get_total_cnt() final;
    uint32_t get_sum_exp() final;
    uint8_t* get_data(int sect) final;

  private:
    uint8_t addr;
    uint8_t sect_cnt;
    uint16_t total_cnt;
    uint32_t sum_exp;
    struct config_sect section[MAX_SECT_NUM];
};

class XDPE152XX : public VR::VoltageRegulator
{
  public:
    explicit XDPE152XX(std::unique_ptr<i2c::I2C> i2cInf) :
        inf(std::move(i2cInf))
    {}

    static std::unique_ptr<VR::VoltageRegulator>
        create(std::unique_ptr<i2c::I2C> inf);

    int get_device_id(uint8_t* dev_id);
    int mfr_fw(uint8_t code, const uint8_t* data,
               uint8_t* resp); // What the hell is MFR??
    int lock_reg();
    int unlock_reg();
    int get_cache_crc(const char* sum_str, uint32_t* sum);
    int get_crc(uint32_t* sum);
    // int programm(struct xdpe152XX_config* config, bool force);

    static int get_config_size(uint8_t product_id, uint8_t rev_code);
    static uint32_t calc_crc(const uint32_t* data, int len);
    // static int check_image(xdpe152XX_config* config);

    int get_remaining_writes(uint8_t* remain);
    int check_image(VR::Config* cfg);
    int programm(VR::Config* cfg, bool force);
    int cache_crc();

    int get_fw_version() final;
    int fw_update(VR::Config* cfg, bool force) final;
    int fw_parse_file(VR::Config& cfg, const uint8_t* image,
                      size_t image_size) final;
    int fw_validate_file(const uint8_t* image, size_t image_size) final;
    int fw_verify() final;

  private:
    std::unique_ptr<i2c::I2C> inf;
};

// End VR namespace
} // namespace VR
