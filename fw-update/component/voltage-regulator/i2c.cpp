#include "i2c.hpp"

#include <stdint.h>

int i2c_rdwr_msg_transfer(int file, uint8_t addr, const uint8_t* tbuf,
                          uint8_t tcount, const uint8_t* rbuf, uint8_t rcount)
{
    (void)file;
    (void)addr;
    (void)tbuf;
    (void)tcount;
    (void)rbuf;
    (void)rcount;
    // TODO
    return 0;
}
