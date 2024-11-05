#pragma once

#include <cstdint>

namespace VR
{

class Config
{
  public:
    virtual ~Config() = default;
    virtual uint8_t* get_data() = 0;

    uint8_t get_address();
    void set_address(uint8_t address);

    uint8_t get_sect_cnt();
    void set_sect_cnt(uint8_t count);

    uint16_t get_total_cnt();
    void set_total_cnt(uint16_t cnt);

    uint32_t get_sum_exp();
    void set_sum_exp(uint32_t sum);


  protected:
    uint8_t address;
    uint8_t sect_cnt;
    uint16_t total_cnt;
    uint32_t sum_exp;
    uint8_t* data;
};

}
