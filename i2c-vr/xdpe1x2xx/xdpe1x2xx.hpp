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

#define MFR_FW_CMD_RESET 0x0e
#define MFR_FW_CMD_RMNG 0x10
#define MFR_FW_CMD_OTP_CONF_STO 0x11
#define MFW_FW_CMD_OTP_FILE_INVD 0x12
#define MFR_FW_CMD_GET_CRC 0x2D

#define XDPE15284C_CONF_SIZE 1344
#define XDPE19283B_CONF_SIZE 1416

#define VR_WARN_REMAINING 3
#define SECT_MAX_CNT 16
#define MAX_SECT_DATA_CNT 200

#define SECT_TRIM 0x02

#define ADDRESS_FIELD  "PMBus Address :"
#define CHECKSUM_FIELD "Checksum :"
#define DATA_START_TAG "[Configuration Data]"
#define DATA_END_TAG   "[End Configuration Data]"
#define DATA_COMMENT   "//"
#define DATA_XV        "XV"
#define END_TAG        "[End]"

#define CRC32_POLY 0xEDB88320
#define VR_RESET_DELAY 500000

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

struct xdpe1x2xxConfig
{
    uint8_t addr;
    uint16_t totalCnt;
    uint32_t sumExp;
    uint8_t sectCnt;
    struct configSect section[SECT_MAX_CNT];
};

class XDPE1X2XX : public VoltageRegulator
{
  public:
    explicit XDPE1X2XX(std::unique_ptr<phosphor::i2c::I2C> i2cInf) :
        I2cInterface(std::move(i2cInf))
    {}

    static std::unique_ptr<VoltageRegulator>
        create(std::unique_ptr<phosphor::i2c::I2C> inf);

    int getDeviceId(uint8_t* dev_id);
    int ctlRegs(bool lock, uint32_t password);
    int mfrFWcmd(uint8_t cmd, uint8_t* data, uint8_t* resp);
    int getRemainingWrites(uint8_t* remain);
    int parseImage(struct xdpe1x2xxConfig *cfg, const uint8_t* image, size_t image_size);
    int checkImage(struct xdpe1x2xxConfig* cfg);
    int reset();
    int program(struct xdpe1x2xxConfig* cfg, bool force);
    int getConfigSize(uint8_t devid, uint8_t rev);

    int fwUpdate(uint8_t* image, size_t image_size, bool force) final;
    int getCRC(uint32_t* sum) final;

  private:
    uint32_t calcCRC32(const uint32_t* data, int len);
    int lineSplit(char** dst, char* src, char* delim);
    std::unique_ptr<phosphor::i2c::I2C> I2cInterface;
};

