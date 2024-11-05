#pragma once

#include "../i2c_interface.hpp"

#include <gmock/gmock.h>

namespace i2c
{

class MockedI2CInterface : public I2CInterface
{
  public:
    virtual ~MockedI2CInterface() = default;

    MOCK_METHOD(void, open, (), (override));
    MOCK_METHOD(bool, isOpen, (), (const, override));
    MOCK_METHOD(void, close, (), (override));

    MOCK_METHOD(void, read, (uint8_t & data), (override));
    MOCK_METHOD(void, read, (uint8_t addr, uint8_t& data), (override));
    MOCK_METHOD(void, read, (uint8_t addr, uint16_t& data), (override));
    MOCK_METHOD(void, read,
                (uint8_t addr, uint8_t& size, uint8_t* data, Mode mode),
                (override));

    MOCK_METHOD(void, write, (uint8_t data), (override));
    MOCK_METHOD(void, write, (uint8_t addr, uint8_t data), (override));
    MOCK_METHOD(void, write, (uint8_t addr, uint16_t data), (override));
    MOCK_METHOD(void, write,
                (uint8_t addr, uint8_t size, const uint8_t* data, Mode mode),
                (override));

    MOCK_METHOD(void, processCall,
                (uint8_t addr, uint16_t writeData, uint16_t& readData),
                (override));
    MOCK_METHOD(void, processCall,
                (uint8_t addr, uint8_t writeSize, const uint8_t* writeData,
                 uint8_t& readSize, uint8_t* readData),
                (override));
};

} // namespace i2c
