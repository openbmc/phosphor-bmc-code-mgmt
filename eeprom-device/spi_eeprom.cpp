#include "spi_eeprom.hpp"

sdbusplus::async::task<bool> SPIEEPROM::resetDevice()
{
    bool res = co_await SPIDevice::resetDevice();

    if (res)
    {
        // Retrieve actual FW version from our chip after successful reset
        softwarePending->setVersion(getVersion());
    }

    co_return res;
}
