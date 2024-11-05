#pragma once

#include "i2c_interface.hpp"

namespace i2c
{

class I2CDevice : public I2CInterface
{
  private:
    I2CDevice() = delete;

    /** @brief Constructor
     *
     * Construct I2CDevice object from the bus id and device address
     *
     * Automatically opens the I2CDevice if initialState is OPEN.
     *
     * @param[in] busId - The i2c bus ID
     * @param[in] devAddr - The device address of the I2C device
     * @param[in] initialState - Initial state of the I2CDevice object
     * @param[in] maxRetries - Maximum number of times to retry an I2C operation
     */
    explicit I2CDevice(uint8_t busId, uint8_t devAddr,
                       InitialState initialState = InitialState::OPEN,
                       int maxRetries = 0) :
        busId(busId), devAddr(devAddr), maxRetries(maxRetries),
        busStr("/dev/i2c-" + std::to_string(busId))
    {
        if (initialState == InitialState::OPEN)
        {
            open();
        }
    }

    /** @brief Invalid file descriptor */
    static constexpr int INVALID_FD = -1;

    /** @brief Empty adapter functionality value with no bit flags set */
    static constexpr unsigned long NO_FUNCS = 0;

    /** @brief The I2C bus ID */
    uint8_t busId;

    /** @brief The i2c device address in the bus */
    uint8_t devAddr;

    /** @brief Maximum number of times to retry an I2C operation */
    int maxRetries = 0;

    /** @brief The file descriptor of the opened i2c device */
    int fd = INVALID_FD;

    /** @brief The i2c bus path in /dev */
    std::string busStr;

    /** @brief Cached I2C adapter functionality value */
    unsigned long cachedFuncs = NO_FUNCS;

    /** @brief Check that device interface is open
     *
     * @throw I2CException if device is not open
     */
    void checkIsOpen() const
    {
        if (!isOpen())
        {
            throw I2CException("Device not open", busStr, devAddr);
        }
    }

    /** @brief Close device without throwing an exception if an error occurs */
    void closeWithoutException() noexcept
    {
        try
        {
            close();
        }
        catch (...)
        {}
    }

    /** @brief Get I2C adapter functionality
     *
     * Caches the adapter functionality value since it shouldn't change after
     * opening the device.
     *
     * @throw I2CException on error
     * @return Adapter functionality value
     */
    unsigned long getFuncs();

    /** @brief Check i2c adapter read functionality
     *
     * Check if the i2c adapter has the functionality specified by the SMBus
     * transaction type
     *
     * @param[in] type - The SMBus transaction type defined in linux/i2c.h
     *
     * @throw I2CException if the function is not supported
     */
    void checkReadFuncs(int type);

    /** @brief Check i2c adapter write functionality
     *
     * Check if the i2c adapter has the functionality specified by the SMBus
     * transaction type
     *
     * @param[in] type - The SMBus transaction type defined in linux/i2c.h
     *
     * @throw I2CException if the function is not supported
     */
    void checkWriteFuncs(int type);

    /** @brief SMBus Block Write-Block Read Process Call using SMBus function
     *
     * In SMBus 2.0 the maximum write size + read size is <= 32 bytes.
     * In SMBus 3.0 the maximum write size + read size is <= 255 bytes.
     * The Linux SMBus function currently only supports the SMBus 2.0 maximum.
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] writeSize - The size of data to write. Write size + read size
     *                        must be <= 32 bytes.
     * @param[in] writeData - The data to write to the i2c device
     * @param[out] readSize - The size of data read from i2c device. Write size
     *                        + read size must be <= 32 bytes.
     * @param[out] readData - Pointer to buffer to hold the data read from the
     *                        i2c device. Must be large enough to hold the data
     *                        returned by the device (max is 32 bytes).
     *
     * @throw I2CException on error
     */
    void processCallSMBus(uint8_t addr, uint8_t writeSize,
                          const uint8_t* writeData, uint8_t& readSize,
                          uint8_t* readData);

    /** @brief SMBus Block Write-Block Read Process Call using I2C messages
     *
     * This method supports block writes of more than 32 bytes.  It can also be
     * used with I2C adapters that do not support the block process call
     * protocol but do support I2C-level commands.
     *
     * This method implements the block process call using the lower level
     * I2C_RDWR ioctl to send I2C messages.  Using this ioctl allows for writes
     * up to 255 bytes.  The write size + read size must be <= 255 bytes.
     *
     * @param[in] addr - The register address of the i2c device
     * @param[in] writeSize - The size of data to write. Write size + read size
     *                        must be <= 255 bytes.
     * @param[in] writeData - The data to write to the i2c device
     * @param[out] readSize - The size of data read from i2c device. Max read
     *                        size is 32 bytes, and write size + read size must
     *                        be <= 255 bytes.
     * @param[out] readData - Pointer to buffer to hold the data read from the
     *                        i2c device. Must be large enough to hold the data
     *                        returned by the device (max is 32 bytes).
     *
     * @throw I2CException on error
     */
    void processCallI2C(uint8_t addr, uint8_t writeSize,
                        const uint8_t* writeData, uint8_t& readSize,
                        uint8_t* readData);

  public:
    /** @copydoc I2CInterface::~I2CInterface() */
    ~I2CDevice()
    {
        if (isOpen())
        {
            // Note: destructors must not throw exceptions
            closeWithoutException();
        }
    }

    /** @copydoc I2CInterface::open() */
    void open();

    /** @copydoc I2CInterface::isOpen() */
    bool isOpen() const
    {
        return (fd != INVALID_FD);
    }

    /** @copydoc I2CInterface::close() */
    void close();

    /** @copydoc I2CInterface::read(uint8_t&) */
    void read(uint8_t& data) override;

    /** @copydoc I2CInterface::read(uint8_t,uint8_t&) */
    void read(uint8_t addr, uint8_t& data) override;

    /** @copydoc I2CInterface::read(uint8_t,uint16_t&) */
    void read(uint8_t addr, uint16_t& data) override;

    /** @copydoc I2CInterface::read(uint8_t,uint8_t&,uint8_t*,Mode) */
    void read(uint8_t addr, uint8_t& size, uint8_t* data,
              Mode mode = Mode::SMBUS) override;

    /** @copydoc I2CInterface::write(uint8_t) */
    void write(uint8_t data) override;

    /** @copydoc I2CInterface::write(uint8_t,uint8_t) */
    void write(uint8_t addr, uint8_t data) override;

    /** @copydoc I2CInterface::write(uint8_t,uint16_t) */
    void write(uint8_t addr, uint16_t data) override;

    /** @copydoc I2CInterface::write(uint8_t,uint8_t,const uint8_t*,Mode) */
    void write(uint8_t addr, uint8_t size, const uint8_t* data,
               Mode mode = Mode::SMBUS) override;

    /** @copydoc I2CInterface::processCall(uint8_t,uint16_t,uint16_t&) */
    void processCall(uint8_t addr, uint16_t writeData,
                     uint16_t& readData) override;

    /** @copydoc I2CInterface::processCall(uint8_t,uint8_t,const
     *                                     uint8_t*,uint8_t&,uint8_t*) */
    void processCall(uint8_t addr, uint8_t writeSize, const uint8_t* writeData,
                     uint8_t& readSize, uint8_t* readData) override;

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
    static std::unique_ptr<I2CInterface>
        create(uint8_t busId, uint8_t devAddr,
               InitialState initialState = InitialState::OPEN,
               int maxRetries = 0);
};

} // namespace i2c
