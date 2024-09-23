#pragma once

#include <stdint.h>

int i2c_rdwr_msg_transfer(int file, uint8_t addr, const uint8_t* tbuf,
                          uint8_t tcount, const uint8_t* rbuf, uint8_t rcount);
