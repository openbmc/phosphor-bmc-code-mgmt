#include "pmbus.hpp"

#include <phosphor-logging/lg2.hpp>

#include <vector>

PageGuard::PageGuard(phosphor::i2c::I2C& i2cInterface) :
    i2cInterface(i2cInterface), originalPage(std::nullopt)
{
    std::vector<uint8_t> tbuf = {static_cast<uint8_t>(PMBusCmd::page)};
    std::vector<uint8_t> rbuf(1);
    if (i2cInterface.sendReceive(tbuf, rbuf))
    {
        originalPage = rbuf[0];
    }
    else
    {
        lg2::error("PageGuard: Failed to read current page, restoration disabled");
    }
}

PageGuard::~PageGuard()
{
    if (originalPage.has_value())
    {
        std::vector<uint8_t> tbuf = {static_cast<uint8_t>(PMBusCmd::page),
                                     *originalPage};
        std::vector<uint8_t> rbuf;

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            lg2::error("PageGuard: Failed to restore page {PAGE}", "PAGE",
                       lg2::hex, *originalPage);
        }
    }
}