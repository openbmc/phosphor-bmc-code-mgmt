#pragma once

#include "interfaces/i2c/i2c.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace VR
{
enum class VRType
{
    XDPE152XX,
    XDPE192XX,
    ISL69260,
};

enum
{
    VR_WARN_REMAIN_WR = 3,
};

class Config
{
  public:
    virtual uint8_t get_address();
    virtual uint8_t get_sect_cnt();
    virtual uint16_t get_total_cnt();
    virtual uint32_t get_sum_exp();
    virtual uint8_t* get_data(int sect);
};

class VoltageRegulator
{
  public:
    virtual int get_fw_version() = 0;
    int get_fw_full_version();
    int get_fw_active_key();
    int vr_fw_update(const uint8_t* image, size_t image_size, bool force);

    virtual int fw_validate_file(const uint8_t* image_file,
                                 size_t image_size) = 0;
    virtual int fw_parse_file(Config& cfg, const uint8_t* image,
                              size_t image_size) = 0;
    virtual int fw_update(Config* cfg, bool force) = 0;
    virtual int fw_verify() = 0;

    static enum VRType stringToEnum(std::string& vrType);

  private:
    std::unique_ptr<i2c::I2C> inf;
};

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus,
                                         uint8_t address);

// namespace VR
} // namespace VR
