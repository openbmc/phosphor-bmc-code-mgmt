#include "xdpe152xx.hpp"

#include "interfaces/i2c/i2c.hpp"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <phosphor-logging/lg2.hpp>

namespace VR
{

const int MAX_KEY_LEN = 64;
const int MAX_VALUE_LEN = 256;

std::unique_ptr<VR::VoltageRegulator>
    XDPE152XX::create(std::unique_ptr<i2c::I2C> inf)
{
    std::unique_ptr<XDPE152XX> dev =
        std::make_unique<XDPE152XX>(std::move(inf));

    return dev;
}

int XDPE152XX::get_fw_full_version(char* ver_str)
{
    (void)ver_str;
    return 0;
}

int XDPE152XX::get_fw_version(char* ver_str)
{
    // const int MAX_VER_STR_LEN = 80;

    char key[MAX_KEY_LEN] = {0};
    char tmp[MAX_KEY_LEN] = {0};

    if (this->get_fw_active_key(key) < 0)
    {
        return -1;
    }

    // key-value_get(key, value, size, flags);
    // Get lock  with key?

    // Stop sensor polling before unlocking registers
    //this->sensor_ctl_stop();

    // Unlock reg
    if (this->unlock_reg() < 0)
    {
        return -1;
    }

    if (this->get_cache_crc(key, NULL) < 0)
    {
        return -1;
    }

    // Lock registers
    if (this->lock_reg() < 0)
    {
        return -1;
    }

    // Start sensor polling again
    // this->sensor_ctl_start();
    // Release instance lock for key

    if (std::snprintf(ver_str, MAX_VER_STR_LEN, "%s", tmp) > (MAX_VER_STR_LEN-1))
    {
        return -1;
    }

    return 0;
}

int XDPE152XX::get_cache_crc(const char* sum_str, uint32_t* sum)
{
    uint8_t remain = 0;
    uint32_t tmp_sum;
    char tmp_str[MAX_VALUE_LEN] = {0};

    if (this->get_remaining_writes(&remain) < 0)
    {
        return -1;
    }

    if (!sum)
    {
        sum = &tmp_sum;
    }

    if (this->get_crc(sum) < 0)
    {
        return -1;
    }

    if (!sum_str)
    {
        sum_str = tmp_str;
    }

    lg2::info("Infineon [SUM], Remaining Writes: [REAMIN]", "SUM", sum,
              "REMAIN", remain);
    return 0;
}

int XDPE152XX::get_crc(uint32_t* sum)
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    if (sum == NULL)
    {
        return -1;
    }

    if (this->mfr_fw(GET_CRC, static_cast<uint8_t*>(tbuf),
                     static_cast<uint8_t*>(rbuf)) < 0)
    {
        return -1;
    }

    std::memcpy(sum, rbuf, sizeof(uint32_t));

    return 0;
}

int XDPE152XX::get_device_id(uint8_t* devID)
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};
    uint8_t addr = PMBUS_IC_DEVICE_ID;
    uint8_t rSize = IFX_IC_DEV_ID_LEN + 1;

    try
    {
        this->inf->send_receive(addr, 0, static_cast<uint8_t*>(tbuf), rSize,
                                static_cast<uint8_t*>(rbuf));
    }
    catch (i2c::I2CException& e)
    {
        lg2::error("getDeviceID failed: {ERR}", "ERR", e.what());
        return -1;
    }

    std::memcpy(devID, &rbuf[1], IFX_IC_DEV_ID_LEN);

    return 0;
}

int XDPE152XX::get_config_size(uint8_t product_id, uint8_t rev_code)
{
    if ((product_id == PRODUCT_ID_XDPE19283) && (rev_code == REV_B))
    {
        return XDPE19283B_CONF_SIZE;
    }

    return XDPE15284C_CONF_SIZE;
}

int XDPE152XX::mfr_fw(uint8_t code, const uint8_t* data, uint8_t* resp)
{
    uint8_t tbuf[16];
    uint8_t rbuf[16];
    uint8_t addr = IFX_MFR_FW_CMD_DATA;
    uint8_t rSize = 0;

    if (data)
    {
        try
        {
            this->inf->send_receive(addr, 4, data, rSize,
                                    static_cast<uint8_t*>(rbuf));
        }
        catch (i2c::I2CException& e)
        {
            lg2::error("mfr_fw failed: {ERR}", "ERR", e.what());
            return -1;
        }
    }

    usleep(300); // why?

    addr = IFX_MFR_FW_CMD;

    try
    {
        this->inf->send_receive(addr, code, static_cast<uint8_t*>(tbuf), rSize,
                                static_cast<uint8_t*>(rbuf));
    }
    catch (i2c::I2CException& e)
    {
        lg2::error("mfr_fw failed: {ERR}", "ERR", e.what());
        return -1;
    }

    usleep(2000);

    if (resp)
    {
        addr = IFX_MFR_FW_CMD_DATA;
        rSize = 6;
        try
        {
            this->inf->send_receive(addr, 0, static_cast<uint8_t*>(tbuf), rSize,
                                    static_cast<uint8_t*>(rbuf));
        }
        catch (i2c::I2CException& e)
        {
            lg2::error("mfr_fw failed: {ERR}", "ERR", e.what());
            return -1;
        }

        if (rbuf[0] != 4)
        {
            lg2::error("unexpected data: {DATA}", "DATA", rbuf[0]);
            return -1;
        }
        std::memcpy(resp, rbuf + 1, 4);
    }

    return 0;
}

int XDPE152XX::get_remaining_writes(uint8_t* remain)
{
    (void)*remain;
    return 0;
}

int XDPE152XX::lock_reg()
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    uint8_t addr = IFX_MFR_DISABLE_SECURITY_ONCE;
    uint8_t tSize = 4; // block write 4 bytes
    uint8_t rSize = 0;
    tbuf[0] = 0x1;     // lock

    try
    {
        this->inf->send_receive(addr, tSize, static_cast<uint8_t*>(tbuf), rSize,
                                static_cast<uint8_t*>(rbuf));
    }
    catch (i2c::I2CException& e)
    {
        lg2::error("lock_reg failed: {ERR}", "ERR", e.what());
        return -1;
    }

    return 0;
}

int XDPE152XX::unlock_reg()
{
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};
    uint8_t addr = IFX_MFR_DISABLE_SECURITY_ONCE;
    uint8_t tSize = 4; // block write 4 bytes
    uint8_t rSize = 0;
    memcpy(&tbuf[0], &REG_LOCK_PASSWORD, 4);
    try
    {
        this->inf->send_receive(addr, tSize, static_cast<uint8_t*>(tbuf), rSize,
                                static_cast<uint8_t*>(rbuf));
    }
    catch (i2c::I2CException& e)
    {
        lg2::error("unlock_reg failed: {ERR}", "ERR", e.what());
        return -1;
    }

    return 0;
}

uint32_t XDPE152XX::calc_crc(const uint32_t* data, int len)
{
    uint32_t crc = 0xFFFFFFFF;
    int i;
    int b;

    if (data == NULL)
    {
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (b = 0; b < 32; b++)
        {
            if (crc & 0x1)
            {
                crc = (crc >> 1) ^ CRC_POLY;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

int XDPE152XX::check_image()
{
    return 0;
}

int XDPE152XX::programm(bool force)
{
    (void)force;
    return 0;
}

int XDPE152XX::cache_crc()
{
    return 0;
}

int XDPE152XX::get_fw_active_key(char* key)
{
    (void)*key;
    return 0;
}

/*
int XDPE152XX::check_image(xdpe152XX_config* config)
{
    uint8_t i;
    uint32_t sum = 0;
    uint32_t crc;

    for(i = 0; i < config->sect_cnt; i++)
    {
        struct config_sect *sect = &config->section[i];
        if (sect == NULL)
        {
            return -1;
        }

        crc = calc_crc(&sect->data[0],2);
        if (crc != sect->data[2])
        {
        //	lg2::error();
            return -1;
        }

        sum += crc;

        crc = calc_crc(&sect->data[3], sect->data_cnt - 4);
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

int XDPE152XX::programm(struct xdpe152XX_config* config, bool force)
{
    uint8_t tbuf[16];
    uint8_t rbuf[16];
    uint8_t remain = 0;
    uint32_t sum = 0;
    int i = 0;
    int j = 0;
    int size = 0;
    int prog = 0;
    int ret = 0;

    if (config == NULL)
    {
        return -1;
    }

    if (get_crc(&sum) < 0)
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
    if (get_remaining_writes(&remain) < 0)
    {
        return -1;
    }

    printf("Remaining writes: %u\n", remain);
    if (!remain)
    {
        // lg2::error();
        return -1;
    }

    if (!force && (remain <= VR::VR_WARN_REMAIN_WR))
    {
        printf("WARNING: the remaining writes is below the threshold value
%d!\n", VR::VR_WARN_REMAIN_WR); printf("Please use \"--force\" option to try
again.\n");
        //syslog(LOG_WARNING, "% insufficient remaining writes %u", __func__,
remain); return -1;
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
            uint8_t addr = PMBUS_STS_CML;
            uint8_t rSize = 0;
            try
            {
                this->inf->send_receive(addr, 0, tbuf, rSize, rbuf);
            }
            catch(i2c::I2CException &e)
            {
                syslog(LOG_WARNING, "%s: Failed to write PMBUS_STS_CML",
__func__); break;
            }

            // invalidate existing data
            tbuf[0] = sect->type;  // section type
            tbuf[1] = 0x00;        // xv0
            tbuf[2] = 0x00;
            tbuf[3] = 0x00;
            if ((ret = mfr_fw(OTP_FILE_INVD, tbuf, NULL)) < 0)
            {
                syslog(LOG_WARNING, "%s: Failed to invalidate %02X", __func__,
sect->type); break;
            }

            usleep(VR_WRITE_DELAY);

            // set scratchpad addr
            tbuf[0] = IFX_MFR_AHB_ADDR;
            tbuf[1] = 4;
            tbuf[2] = 0x00;
            tbuf[3] = 0xe0;
            tbuf[4] = 0x05;
            tbuf[5] = 0x20;
            if ((ret = VoltageRegulator::send_receive(tbuf, 6, rbuf, 0)) < 0)
            {
                syslog(LOG_WARNING, "%s: Failed to set scratchpad addr",
__func__); break;
            }
            usleep(VR_WRITE_DELAY);
            size = 0;
        }

        // program data into scratch
        for (j = 0; j < sect->data_cnt; j++)
        {
            tbuf[0] = IFX_MFR_REG_WRITE;
            tbuf[1] = 4;
            std::memcpy(&tbuf[2], &sect->data[j], 4);
            if ((ret = VoltageRegulator::send_receive(tbuf, 6, rbuf, 0)) < 0)
            {
                syslog(LOG_WARNING, "%s: Failed to write data %08X", __func__,
sect->data[j]); break;
            }
            usleep(VR_WRITE_DELAY);
        }
        if (ret)
        {
            break;
        }

        size += sect->data_cnt * 4;
        if ((i+1 >= config->sect_cnt) || (sect->type !=
config->section[i+1].type))
        {
            // upload scratchpad to OTP
            std::memcpy(tbuf, &size, 2);
            tbuf[2] = 0x00;
            tbuf[3] = 0x00;
            if ((ret = mfr_fw(OTP_CONF_STO, tbuf, NULL)) < 0)
            {
                syslog(LOG_WARNING, "%s: Failed to upload data to OTP",
__func__); break;
            }

            // wait for programming soak (2ms/byte, at least 200ms)
            // ex: Config (604 bytes): (604 / 50) + 2 = 14 (1400 ms)
            size = (size / 51) + 2;
            for (j = 0; j < size; j++)
            {
                usleep(10000);
            }

            tbuf[0] = PMBUS_STS_CML;
            if ((ret = VoltageRegulator::send_receive(tbuf, 1, rbuf, 1)) < 0)
            {
                syslog(LOG_WARNING, "%s: Failed to read PMBUS_STS_CML",
__func__); break;
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
*/

int XDPE152XX::fw_update(bool force)
{
    uint8_t remain = 0;
    this->check_image();
    this->programm(force);
    this->get_remaining_writes(&remain);
    // kv_set(ver_key, value, 0, KV_FPERSIST); - What for?
    return 0;
}

int XDPE152XX::fw_validate_file(const uint8_t* image, size_t image_size)
{
    (void)image;
    (void)image_size;
    return 0;
}

int XDPE152XX::fw_parse_file(const uint8_t* image,
                             size_t image_size)
{
    const size_t len_start = strlen(DATA_START_TAG);
    const size_t len_end = strlen(DATA_END_TAG);
    const size_t len_comment = strlen(DATA_COMMENT);
    const size_t len_xv = strlen(DATA_XV);
    int i = 0;
    int data_cnt = 0;
    int sect_idx = -1;
    uint8_t sect_type = 0;
    uint16_t offset = 0;
    uint32_t dword = 0;
    bool is_data = false;
    FILE* fp = NULL;
    char line[128];
    char *token = NULL;
    std::string path = "/tmp/vr_update";

    std::fstream vr_update_file;
    vr_update_file.open(path);
    //NOLINTBEGIN
    vr_update_file.write((char*)image, image_size);
    //NOLINTEND
    vr_update_file.close();

    fp = fopen(path.c_str(), "r");

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (!strncmp(line, DATA_COMMENT, len_comment))
        {
            token = line + len_comment;
            if (!strncmp(token, DATA_XV, len_xv))
            { // find "//XV..."
                printf("Parsing %s", token);
            }
            continue;
        }
        else if (!strncmp(line, DATA_END_TAG, len_end))
        { // end of data
            break;
        }
        else if (is_data)
        {
            char* token_list[8] = {0};
            char delim = ' ';
            int token_size =
                pal_file_line_split(token_list, line, &delim, DATA_LEN_IN_LINE);
            if (token_size < 1)
            { // unexpected data line
                continue;
            }

            offset = (uint16_t)strtol(token_list[0], NULL, 16);
            if (sect_type == SECT_TRIM && offset != 0x0)
            { // skip Trim
                continue;
            }

            for (i = 1; i < token_size; i++)
            { // DWORD0 ~ DWORD3
                dword = (uint32_t)strtoul(token_list[i], NULL, 16);
                if ((offset == 0x0) && (i == 1))
                {                               // start of section header
                    sect_type = (uint8_t)dword; // section type
                    if (sect_type == SECT_TRIM)
                    {
                        break;
                    }

                    if ((++sect_idx) >= MAX_SECT_NUM)
                    {
                        syslog(LOG_WARNING, "%s: Exceed max section number",
                               __func__);
                        fclose(fp);
                        return -1;
                    }

                    this->config.section[sect_idx].type = sect_type;
                    this->config.sect_cnt = sect_idx + 1;
                    data_cnt = 0;
                }
                if (data_cnt >= MAX_SECT_DATA_NUM)
                {
                    syslog(LOG_WARNING, "%s: Exceed max data count", __func__);
                    fclose(fp);
                    return -1;
                }
                this->config.section[sect_idx].data[data_cnt++] = dword;
                this->config.section[sect_idx].data_cnt = data_cnt;
                this->config.total_cnt++;
            }
        }
        else
        {
            if ((token = strstr(line, ADDRESS_FIELD)) != NULL)
            {
                if ((token = strstr(token, "0x")) != NULL)
                {
                    this->config.address= (uint8_t)(strtoul(token, NULL, 16) << 1);
                }
            }
            else if ((token = strstr(line, CHECKSUM_FIELD)) != NULL)
            {
                if ((token = strstr(token, "0x")) != NULL)
                {
                    this->config.sum_exp = (uint32_t)strtoul(token, NULL, 16);
                    printf("Configuration Checksum: %08X\n", this->config.sum_exp);
                }
            }
            else if (!strncmp(line, DATA_START_TAG, len_start))
            {
                is_data = true;
                continue;
            }
            else
            {
                continue;
            }
        }
    }
    fclose(fp);
    return 0;
}

int XDPE152XX::fw_verify()
{
    this->mfr_fw(FW_RESET, NULL, NULL);
    usleep(VR_RELOAD_DELAY);
    this->cache_crc();
    return 0;
}

int pal_file_line_split(char** dst, char* src, char* delim, int max_size)
{
    int size = 0;
    char* s = std::strtok(src, delim);

    while (s)
    {
        *dst++ = s;
        if ((++size) >= max_size)
        {
            break;
        }
        s = std::strtok(NULL, delim);
    }
    return size;
}

} // namespace VR
