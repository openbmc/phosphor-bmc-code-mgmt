#include "xdpe152xx.hpp"
#include "i2c/i2c_interface.hpp"


#include <phosphor-logging/lg2.hpp>

XDPE152XX::XDPE152XX(I2CInterface& i2cInf) : {}

int XDPE152XX::getDeviceID(uint8_t* devID)
{
	uint8_t tbuf[16], rbuf[16];
	
	tbuf[0] = PMBUS_IC_DEVICE_ID;

	try 
	{
		//this.inf.readwrite();
	}
	catch (I2CException e) {
		lg2::error("getDeviceID failed: {ERR}", "ERR", e.what());
		return -1;
	}

	std::memcpy(devID, &rbuf[1], IFX_IC_DEV_ID_LEN);

	return 0;
}

int XDPE152XX::getConfigSize(uint8_t product_id, uint8_t rev_code)
{
	if (product_id == PRODUCT_ID_XDPE19283) && (rev_code == REV_B)
	{
		return XDPE19283B_CONF_SIZE;
	}

	return XDPE15284C_CONF_SIZE;
}

int XDPE152XX::mfr_fw(uint8_t code, uint8_t* data, uint8_t* resp)
{
	uint8_t tbuf[16], rbuf[16];

	if (data) 
	{
		tbuf[0] = IFX_MFW_FW_CMD_DATA;
		tbuf[1] = 4; // 4 blocks to write
		std::memcpy(&tbuf[2], data, 4);

		try
		{
			// this.inf.readwrite();
		}
		catch(I2CException e)
		{
			lg2::error("mfr_fw failed: {ERR}", "ERR", e.what());
			return -1;
		}
	}

	std::usleep(300); // why?
					  
	tbuf[0] = IFX_MFW_FW_CMD;
	tbuf[1] = code;

	try 
	{
		// this.inf.readwrite();
	}
	catch(I2CException e)
	{
		lg2::error("mfr_fw failed: {ERR}", "ERR", e.what());
		return -1;
	}

	std::msleep(20);

	if (resp)
	{
		tbuf[0] = IFX_MFR_FW_CMD_DATA;
		
		try
		{
			// this.inf.readwrite();
		}
		catch(I2CException e)
		{
			lg2::error("mfr_fw failed: {ERR}", "ERR", e.what());
			return -1;
		}

		if (rbuf[0] != 4)
		{
			lg2::error("unexpected data: {DATA}", "DATA", rbuf[0]);
			return -1;
		}
		std::memcpy(resp, rbuf+1, 4);
	}

	return 0;
}

int XDPE152XX::lock_reg()
{
	uint8_t tbuf[16] = {0}, rbuf[16] = {0};
	
	tbuf[0] = IFX_MFR_DISABLE_SECURITY_ONCE;
	tbuf[1] = 4;  // block write 4 bytes
	tbuf[2] = 0x1; // lock

	try
	{
		// this.inf.readwrite();
	}
	catch(I2CException e)
	{
		lg2::error("lock_reg failed: {ERR}", "ERR", e.what());
		return -1;
	}

	return 0;
}

int XDPE152XX::unlock_reg()
{
	uint8_t tbuf[16] = {0}, rbuf[16] = {0};
	tbuf[0] = IFX_MFR_DISABLE_SECURITY_ONCE;
	tbuf[1] = 4;  // block write 4 bytes
	
	try
	{
		// this.inf.readwrite();
	}
	catch(I2CException e)
	{
		lg2::error("unlock_reg failed: {ERR}", "ERR", e.what());
		return -1;
	}

	return 0;
}

int XDPE152XX::cache_crc(char* key, char* sum_str, uint32_t* sum)
{
	uint8_t remain;
	uint32_t tmp_sum;
	char tmp_str[MAX_VALUE_LEN] = {0};

	if (key == NULL}
	{
		lg2::error("cache_crc failed: key argument is NULL");
		return -1
	}

	try
	{
		// this.get_remaining_writes(&remain)
	}
	catch(I2CException e)
	{
		lg2::error("cache_crc failed: {ERR}", "ERR", e.what());
		return -1;
	}

	if (!sum)
	{
		sum = &tmp_sum;
	}

	try
	{
		// this.get_crc(sum);
	}
	catch(I2CException e)
	{
		lg2::error("cache_crc failed: {ERR}","ERR", e.what());
		return -1;
	}

	if (!sum_str)
	{	
		sum_str = tmp_str;
	}

	std::print(std::stdout,"Infineon {0}, Remaining Writes: {1}", sum, remain);
	// kv_set(key, sum, 0,0); // What is this?

	return 0;
}

int XDPE152XX::calc_crc(uint32_t* const data, int len)
{
	uint32_t crc = 0xFFFFFFFF;
	int i,b;

	if (data == NULL)
	{
		return 0;
	}

	for(i = 0; i < len; i++)
	{
		crc ^= data[i];
		for(b = 0; b < 32; b++)
		{
			if(crc & 0x1)
			{
				crc = (crc>>1) ^ CRC_POLY;
			}
			else 
			{
				crc >>= 1;
			}
		}
	}

	return ~crc;
}

static int XDPE152XX::check_image(xdpe152XX_config* config)
{
	uint8_t i;
	uint32_t sum = 0, crc;

	for(i = 0; i < config->sect_cnt; i++)
	{
		struct config_sect *sect = &config->config_sect[i];
		if (sect == NULL)
		{
			return -1;
		}

		crc = calc_crc32(&sect->data[0],2);
		if (crc != sect->data[2])
		{
			lg2::error();
			return -1;
		}

		sum += crc;

		crc = calc_crc32(&sect->data[3], sect->data_cnt - 4);
		if (crc != sect->data[sect->data_cnt-1])
		{
			return -1;
		}
		sum += crc;

	}

	lg2::info("File CRC: {CRC}","CRC", crc);
	if (sum != config->sum_exp)
	{
		lg2::error("Checksum mismatch: Expected: {EXP}, Actual: {ACT}","EXP",
				config->sum_exp, "ACT", crc);
		return -1;
	}

	return 0;
}

int XDPE152XX::programm()
{
	uint8_t tbuf[16], rbuf[16];
	uint8_t remain = 0;
	uint32_t sum = 0;
	int i, j, size = 0;
	int prog = 0, ret = 0;

	if (config == NULL) 
	{
    	return -1;
  	}

	if (get_xdpe152xx_crc(bus, addr, &sum) < 0) 
	{
    	return -1;
	}

	if (!force && (sum == config->sum_exp)) 
	{
    	printf("WARNING: the Checksum is the same as used now %08X!\n", sum);
    	printf("Please use \"--force\" option to try again.\n");
    	syslog(LOG_WARNING, "%s: redundant programming", __func__);
    	return -1;
	}

	// check remaining writes
  	if (get_xdpe152xx_remaining_wr(bus, addr, &remain) < 0)
	{
    	return -1;
  	}

	printf("Remaining writes: %u\n", remain);
	if (!remain) 
	{
    	syslog(LOG_WARNING, "%s: no remaining writes", __func__);
    	return -1;
	}

	if (!force && (remain <= VR_WARN_REMAIN_WR))
	{
    	printf("WARNING: the remaining writes is below the threshold value %d!\n",
           VR_WARN_REMAIN_WR);
    	printf("Please use \"--force\" option to try again.\n");
    	syslog(LOG_WARNING, "%s: insufficient remaining writes %u", __func__, remain);
    	return -1;
	}

	for (i = 0; i < config->sect_cnt; i++) 
	{
    	struct config_sect *sect = &config->section[i];
    	if (sect == NULL) 
		{
      		ret = -1;
      		break;
   		}

    	if ((i <= 0) || (sect->type != config->section[i-1].type)) 
		{
      		// clear bit0 of PMBUS_STS_CML
      		tbuf[0] = PMBUS_STS_CML;
      		tbuf[1] = 0x01;
      		if ((ret = vr_xfer(bus, addr, tbuf, 2, rbuf, 0)) < 0)
			{
        		syslog(LOG_WARNING, "%s: Failed to write PMBUS_STS_CML", __func__);
        		break;
      		}

      		// invalidate existing data
      		tbuf[0] = sect->type;  // section type
      		tbuf[1] = 0x00;        // xv0
      		tbuf[2] = 0x00;
      		tbuf[3] = 0x00;
      		if ((ret = xdpe152xx_mfr_fw(bus, addr, OTP_FILE_INVD, tbuf, NULL)) < 0) 
			{
        		syslog(LOG_WARNING, "%s: Failed to invalidate %02X", __func__, sect->type);
        		break;
      		}
      		
			msleep(VR_WRITE_DELAY);

			// set scratchpad addr
      		tbuf[0] = IFX_MFR_AHB_ADDR;
      		tbuf[1] = 4;
      		tbuf[2] = 0x00;
      		tbuf[3] = 0xe0;
      		tbuf[4] = 0x05;
      		tbuf[5] = 0x20;
      		if ((ret = vr_xfer(bus, addr, tbuf, 6, rbuf, 0)) < 0)
			{
        		syslog(LOG_WARNING, "%s: Failed to set scratchpad addr", __func__);
        		break;
      		}
      		msleep(VR_WRITE_DELAY);
      		size = 0;
    	}

    	// program data into scratch
    	for (j = 0; j < sect->data_cnt; j++) 
		{
      		tbuf[0] = IFX_MFR_REG_WRITE;
      		tbuf[1] = 4;
      		std::memcpy(&tbuf[2], &sect->data[j], 4);
      		if ((ret = vr_xfer(bus, addr, tbuf, 6, rbuf, 0)) < 0) {
        		syslog(LOG_WARNING, "%s: Failed to write data %08X", __func__, sect->data[j]);
        		break;
      		}
      		msleep(VR_WRITE_DELAY);
    	}
    	if (ret)
		{
     		break;
    	}

    	size += sect->data_cnt * 4;
    	if ((i+1 >= config->sect_cnt) || (sect->type != config->section[i+1].type)) 
		{
      		// upload scratchpad to OTP
			std::memcpy(tbuf, &size, 2);
      		tbuf[2] = 0x00;
      		tbuf[3] = 0x00;
      		if ((ret = xdpe152xx_mfr_fw(bus, addr, OTP_CONF_STO, tbuf, NULL)) < 0)
			{
        		syslog(LOG_WARNING, "%s: Failed to upload data to OTP", __func__);
        		break;
      		}

      		// wait for programming soak (2ms/byte, at least 200ms)
      		// ex: Config (604 bytes): (604 / 50) + 2 = 14 (1400 ms)
      		size = (size / 50) + 2;
      		for (j = 0; j < size; j++) 
			{
        		msleep(100);
      		}

      		tbuf[0] = PMBUS_STS_CML;
      		if ((ret = vr_xfer(bus, addr, tbuf, 1, rbuf, 1)) < 0) 
			{
        		syslog(LOG_WARNING, "%s: Failed to read PMBUS_STS_CML", __func__);
        		break;
      		}
      
			if (rbuf[0] & 0x01) 
			{
        		syslog(LOG_WARNING, "%s: CML Other Memory Fault: %02X (%02X)",
               		__func__, rbuf[0], sect->type);
        	ret = -1;
        	break;
      		}
    	}

    	prog += sect->data_cnt;
		printf("\rupdated: %d %%  ", (prog*100)/config->total_cnt);
    	fflush(stdout);
	}
	
	if (ret) {
		return -1;
	}

	return 0;
}

int XDPE152XX::get_version(){
	return 0;
}

int XDPE152XX::parse_file()
{
	return 0;
}

int XDPE152XX::fw_update()
{
	return 0;
}

int XDPE152XX::fw_verify()
{
	return 0;
}

