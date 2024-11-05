#pragma once

#include "fw-update/component/regulators/vr.hpp"
#include "fw-update/component/interfaces/i2c/i2c_interface.hpp"
#include <cstdint>

namespace VR
{

const int MAX_SEC_DATA = 200;
const int MAX_SECT_NUM = 16;
const int IFX_IC_DEV_ID_LEN = 2;

enum {
	PMBUS_STS_CML = 0x7E,
	PMBUS_IC_DEVICE_ID = 0xAD,
	IFX_MFR_DISABLE_SECURITY_ONCE = 0xCB,
	IFX_MFR_AHB_ADDR    = 0xCE,
	IFX_MFR_REG_WRITE   = 0xDE,
	IFX_MFR_REG_READ    = 0xDF,
	IFX_MFR_FW_CMD_DATA = 0xFD,
	IFX_MFR_FW_CMD      = 0xFE,

	FW_RESET      = 0x0e,
	OTP_PTN_RMNG  = 0x10,
	OTP_CONF_STO  = 0x11,
	OTP_FILE_INVD = 0x12,
	GET_CRC       = 0x2D,
    XDPE15284C_CONF_SIZE = 1344,
    XDPE19283B_CONF_SIZE = 1416,
    CRC_POLY = 0xEDB88320,  // polynomial
    VR_WRITE_DELAY = 1000,
    VR_RELOAD_DELAY = 50000,
};

const uint32_t REG_LOCK_PASSWORD = 0x7F48680C;

enum revision_code {
  REV_A = 0x00,
  REV_B,
  REV_C,
  REV_D,
};

enum product_id {
	PRODUCT_ID_XDPE15284 = 0x8A,
	PRODUCT_ID_XDPE19283 = 0x95,
};

struct config_sect {
	uint8_t type;
	uint16_t data_cnt;
	uint32_t data[MAX_SEC_DATA];
};

struct xdpe152XX_config {
	uint8_t addr;
	uint8_t sect_cnt;
	uint16_t total_cnt;
	uint32_t sum_exp;
	struct config_sect section[MAX_SECT_NUM];
};

class XDPE152XX : public VR::VoltageRegulator
{
	public:
        ~XDPE152XX() override;

        explicit XDPE152XX(std::unique_ptr<i2c::I2CInterface> i2cInf) : inf(std::move(i2cInf)){}

        static std::unique_ptr<VR::VoltageRegulator> create(std::unique_ptr<i2c::I2CInterface> inf);

        virtual int get_fw_version() final;
        int get_device_id(uint8_t* dev_id);
        int mfr_fw(uint8_t code,const uint8_t* data, uint8_t* resp); // What the hell is MFR??
		int lock_reg();
		int unlock_reg();
		int get_cache_crc(const char* sum_str, uint32_t* sum);
		int get_crc(uint32_t* sum);
       // int programm(struct xdpe152XX_config* config, bool force);

        static int get_config_size(uint8_t product_id, uint8_t rev_code);
        static uint32_t calc_crc(const uint32_t* data, int len);
       // static int check_image(xdpe152XX_config* config);

        int get_remaining_writes(uint8_t* remain);
    private:
        std::unique_ptr<i2c::I2CInterface> inf;

};

// End VR namespace
}
