#pragma once

#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace i2c
{

class I2CException : public std::exception
{
  public:
    explicit I2CException(const std::string& info, const std::string& bus,
                          uint8_t addr, int errorCode = 0) :
        bus(bus), addr(addr), errorCode(errorCode)
    {
        std::stringstream ss;
        ss << "I2CException: " << info << ": bus " << bus << ", addr 0x"
           << std::hex << static_cast<int>(addr);
        if (errorCode != 0)
        {
            ss << std::dec << ", errno " << errorCode << ": "
               << strerror(errorCode);
        }
        errStr = ss.str();
    }
    virtual ~I2CException() = default;

    const char* what() const noexcept override
    {
        return errStr.c_str();
    }
    std::string bus;
    uint8_t addr;
    int errorCode;
    std::string errStr;
};

class I2CInterface
{
  public:
    /** @brief Destructor
     *
     * Closes the I2C interface to the device if necessary.
     */
    virtual ~I2CInterface() = default;

    /** @brief Initial state when an I2CInterface object is created */
    enum class InitialState
    {
        OPEN,
        CLOSED
    };

    /** @brief The block transaction mode */
    enum class Mode
    {
        SMBUS,
        I2C,
    };

    /** @brief Open the I2C interface to the device
     *
     * Throws an I2CException if the interface is already open.  See isOpen().
     *
     * @throw I2CException on error
     */
    virtual void open() = 0;

    /** @brief Indicates whether the I2C interface to the device is open
     *
     * @return true if interface is open, false otherwise
     */
    virtual bool isOpen() const = 0;

    /** @brief Close the I2C interface to the device
     *
     * The interface can later be re-opened by calling open().
     *
     * Note that the destructor will automatically close the I2C interface if
     * necessary.
     *
     * Throws an I2CException if the interface is not open.  See isOpen().
     *
     * @throw I2CException on error
     */
    virtual void close() = 0;

    /** @brief Read byte data from i2c
     *
     * @param[out] data - The data read from the i2c device
     *
     * @throw I2CException on error
     */
    virtual void read(uint8_t& data) = 0;

    /** @brief Read byte data from i2c
     *
     * @param[in] addr - The register address of the i2c device
     * @param[out] data - The data read from the i2c device
     *
     * @throw I2CException on error
     */
    virtual void read(uint8_t addr, uint8_t& data) = 0;

    /** @brief Read word data from i2c
     *
     * Uses the SMBus Read Word protocol.  Reads two bytes from the device, and
     * the first byte read is the low-order byte.
     *
     * @param[in] addr - The register address of the i2c device
     * @param[out] data - The data read from the i2c device
     *
     * @throw I2CException on error
     */
    virtual void read(uint8_t addr, uint16_t& data) = 0;

    /** @brief Read block data from i2c
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in,out] size - The out parameter to indicate the size of data read
     *                       from i2c device; when mode is I2C, it is also the
     *                       input parameter to indicate how many bytes to read
     * @param[out] data - The pointer to buffer to read from the i2c device,
     *                    the buffer shall be big enough to hold the data
     *                    returned by the device. SMBus allows at most 32
     *                    bytes.
     * @param[in] mode - The block read mode, either SMBus or I2C.
     *                   Internally, it invokes functions from libi2c:
     *                    * For SMBus: i2c_smbus_read_block_data()
     *                    * For I2C: i2c_smbus_read_i2c_block_data()
     *
     * @throw I2CException on error
     */
    virtual void read(uint8_t addr, uint8_t& size, uint8_t* data,
                      Mode mode = Mode::SMBUS) = 0;

    /** @brief Write byte data to i2c
     *
     * @param[in] data - The data to write to the i2c device
     *
     * @throw I2CException on error
     */
    virtual void write(uint8_t data) = 0;

    /** @brief Write byte data to i2c
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] data - The data to write to the i2c device
     *
     * @throw I2CException on error
     */
    virtual void write(uint8_t addr, uint8_t data) = 0;

    /** @brief Write word data to i2c
     *
     * Uses the SMBus Write Word protocol.  Writes two bytes to the device, and
     * the first byte written is the low-order byte.
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] data - The data to write to the i2c device
     *
     * @throw I2CException on error
     */
    virtual void write(uint8_t addr, uint16_t data) = 0;

    /** @brief Write block data to i2c
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] size - The size of data to write, SMBus allows at most 32
     *                   bytes
     * @param[in] data - The data to write to the i2c device
     * @param[in] mode - The block write mode, either SMBus or I2C.
     *                   Internally, it invokes functions from libi2c:
     *                    * For SMBus: i2c_smbus_write_block_data()
     *                    * For I2C: i2c_smbus_write_i2c_block_data()
     *
     * @throw I2CException on error
     */
    virtual void write(uint8_t addr, uint8_t size, const uint8_t* data,
                       Mode mode = Mode::SMBUS) = 0;

    /** @brief SMBus Process Call protocol to write and then read word data
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] writeData - The data to write to the i2c device
     * @param[out] readData - The data read from the i2c device
     *
     * @throw I2CException on error
     */
    virtual void processCall(uint8_t addr, uint16_t writeData,
                             uint16_t& readData) = 0;

    /** @brief SMBus Block Write-Block Read Process Call protocol
     *
     * The maximum write size depends on the SMBus version being used and what
     * functionality the I2C adapter supports.
     *
     * If SMBus version 2.0 is being used, the maximum write size is 32 bytes.
     * The read size + write size must be <= 32 bytes.
     *
     * If SMBus version 3.0 is being used and the I2C adapter supports plain
     * I2C-level commands, the maximum write size is 255 bytes. The read size +
     * write size must be <= 255 bytes.
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] writeSize - The size of data to write
     * @param[in] writeData - The data to write to the i2c device
     * @param[out] readSize - The size of data read from i2c device. Max read
     *                        size is 32 bytes.
     * @param[out] readData - Pointer to buffer to hold the data read from the
     *                        i2c device. Must be large enough to hold the data
     *                        returned by the device (max is 32 bytes).
     *
     * @throw I2CException on error
     */
    virtual void processCall(uint8_t addr, uint8_t writeSize,
                             const uint8_t* writeData, uint8_t& readSize,
                             uint8_t* readData) = 0;
};

/** @brief Create an I2CInterface instance
 *
 * Automatically opens the I2CInterface if initialState is OPEN.
 *
 * @param[in] busId - The i2c bus ID
 * @param[in] devAddr - The device address of the i2c
 * @param[in] initialState - Initial state of the I2CInterface object
 * @param[in] maxRetries - Maximum number of times to retry an I2C operation
 *
 * @return The unique_ptr holding the I2CInterface
 */
std::unique_ptr<I2CInterface> create(
    uint8_t busId, uint8_t devAddr,
    I2CInterface::InitialState initialState = I2CInterface::InitialState::OPEN,
    int maxRetries = 0);

} // namespace i2c
