#include "mocked_i2c_interface.hpp"

#include <memory>

namespace i2c
{

std::unique_ptr<I2CInterface>
    create(uint8_t /*busId*/, uint8_t /*devAddr*/,
           I2CInterface::InitialState /*initialState*/, int /*maxRetries*/)
{
    return std::make_unique<MockedI2CInterface>();
}

} // namespace i2c
