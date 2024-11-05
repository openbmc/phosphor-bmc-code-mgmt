#pragma once

#include <cstdint>

usage namespace std;

const int MAX_SEC_DATA = 200;
const int MAC_SECT_NUM = 16;
const int IFX_IC_DEV_ID_LEN = 2;

enum {
	PMBUS_STS_CML = 0x7E
	PMBUS_IC_DEVICE_ID = 0xAD
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
}

enum revision_code{
  REV_A = 0x00,
  REV_B,
  REV_C,
  REV_D,
};

enum product_id {
	PRODUCT_ID_XDPE15284 = 0x8A;
	PRODUCT_ID_XDPE19283 = 0x95;
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

class XDPE152XX: public VoltageRegulator 
{
	public:
		XDPE152XX(I2CInterface& i2cInf);
	
	private:
		I2CInterface* inf;
		int get_device_id();
		int get_config_size();
		int mfr_fw(); // What the hell is MFR??
		int lock_reg();
		int unloc_reg();
		int cache_crc();
		int32_t calc_crc();
		static int check_image();
		int programm();
};
