#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

namespace phosphor::software::VR
{

/**
 * @brief
 * Columns of an Automated Test Equipment (ATE) format configuration file.
 * Each enumerator corresponds to a tab-separated column index.
 */
enum class ATE : uint8_t
{
    configId = 0,
    pageNum,
    regAddrHex,
    regAddrDec,
    regName,
    regDataHex,
    regDataDec,
    writeType,
    colCount,
};

enum class MPSPage : uint8_t
{
    page0 = 0,
    page1,
    page2,
    page3,
    page4,
    page5,
    page6,
    page7,
    page29 = 0x29,
    page2A = 0x2A,
};

struct MPSData
{
    uint8_t page = 0;
    uint8_t addr = 0;
    uint8_t length = 0;
    std::array<uint8_t, 4> data{};
};

struct MPSConfig
{
    uint32_t vendorId = 0;
    uint32_t productId = 0;
    uint32_t configId = 0;
    uint32_t crcUser = 0;
    uint32_t crcMulti = 0;
    std::vector<MPSData> registersData;
};

/**
 * @brief
 * Utility class to iterate over lines and tokenize them by tab characters.
 */
class TokenizedLines
{
  public:
    TokenizedLines(const uint8_t* d, size_t s) :
        data(reinterpret_cast<const char*>(d), s)
    {}
    /**
     * @brief Iterator over tokenized lines.
     */
    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::vector<std::string_view>;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        Iterator() = default; // End iterator
        Iterator(std::string_view sv) : remaining(sv)
        {
            next();
        }

        reference operator*() const
        {
            return currentTokens;
        }

        pointer operator->() const
        {
            return &currentTokens;
        }

        Iterator& operator++()
        {
            next();
            return *this;
        }

        Iterator operator++(int)
        {
            auto result = *this;
            ++(*this);
            return result;
        }

        friend bool operator==(const Iterator& a, const Iterator& b)
        {
            return a.remaining.empty() && b.remaining.empty();
        }

        friend bool operator!=(const Iterator& a, const Iterator& b)
        {
            return !(a == b);
        }

      private:
        std::string_view remaining;
        std::vector<std::string_view> currentTokens;

        void next()
        {
            currentTokens.clear();
            if (remaining.empty())
            {
                return;
            }

            // Extract current line
            auto newlinePos = remaining.find('\n');
            std::string_view line = remaining.substr(0, newlinePos);
            remaining = (newlinePos == std::string_view::npos)
                            ? std::string_view{}
                            : remaining.substr(newlinePos + 1);

            // Tokenize by tab
            size_t start = 0;
            while (start < line.size())
            {
                start = line.find_first_not_of('\t', start);
                if (start == std::string_view::npos)
                {
                    break;
                }

                auto end = line.find('\t', start);
                currentTokens.emplace_back(line.substr(start, end - start));
                start = (end == std::string_view::npos) ? line.size() : end;
            }
        }
    };

    Iterator begin() const
    {
        return Iterator(data);
    }

    static Iterator end()
    {
        return Iterator();
    }

  private:
    std::string_view data;
};

/**
 * @brief Base parser for MPS configuration images.
 */
class MPSImageParser
{
  public:
    MPSImageParser(const uint8_t* image, size_t imageSize) :
        lineTokens(image, imageSize)
    {}

    virtual ~MPSImageParser() = default;
    MPSImageParser(const MPSImageParser&) = delete;
    MPSImageParser& operator=(const MPSImageParser&) = delete;
    MPSImageParser(MPSImageParser&&) = default;
    MPSImageParser& operator=(MPSImageParser&&) = default;

    template <typename>
    inline static constexpr bool always_false = false;

    /**
     * @brief Extract a typed value from a tokenized line.
     * @tparam T Return type (string or integral type)
     * @param tokens Tokenized line
     * @param index Column index (ATE enum)
     * @return Parsed value or default if invalid
     */
    template <typename T>
    T getVal(const std::vector<std::string_view>& tokens, ATE index)
    {
        size_t idx = static_cast<size_t>(index);

        if (tokens.size() <= idx)
        {
            lg2::error("Index out of range for ATE enum: {INDEX}", "INDEX",
                       static_cast<uint32_t>(idx));
            return T{};
        }

        std::string_view token = tokens[idx];

        if constexpr (std::is_same_v<T, std::string>)
        {
            return std::string(token);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            unsigned long val = 0;
            try
            {
                val = std::stoul(std::string(token), nullptr, 16);
            }
            catch (...)
            {
                lg2::error("Invalid hex value: {INDEX}", "INDEX",
                           static_cast<uint32_t>(idx));
                return T{};
            }
            return static_cast<T>(val);
        }
        else
        {
            static_assert(always_false<T>, "Unsupported type in getVal");
        }
    }

    /**
     * @brief Check if a tokenized line contains valid register data.
     */
    static bool isValidDataTokens(const std::vector<std::string_view>& tokens);

    /**
     * @brief Convert tokenized line into MPSData structure.
     */
    virtual MPSData extractData(const std::vector<std::string_view>& tokens);

    /**
     * @brief Collect all register data entries from the parsed image.
     */
    std::vector<MPSData> getRegistersData();

    TokenizedLines lineTokens;
};

/**
 * @brief Base class for MPS Voltage Regulators.
 */
class MPSVoltageRegulator : public VoltageRegulator
{
  public:
    MPSVoltageRegulator(sdbusplus::async::context& ctx, uint16_t bus,
                        uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
    {}

    /**
     * @brief Parse device-specific configuration from the loaded image.
     * @return async task returning true if parsing succeeds
     */
    virtual sdbusplus::async::task<bool> parseDeviceConfiguration() = 0;

    /**
     * @brief Parse an image file into internal MPS configuration.
     * @param image Pointer to the image data
     * @param imageSize Size of the image data
     * @return async task returning true if parsing succeeds
     */
    sdbusplus::async::task<bool> parseImage(
        const uint8_t* image, size_t imageSize,
        std::unique_ptr<MPSImageParser> customParser = nullptr);

    /**
     * @brief Group register data by page, optionally masked and shifted.
     * @param configMask Bitmask to select relevant page bits (default 0xFF)
     * @param shift Number of bits to shift masked value to obtain group key
     * @return map of page keys to vector of MPSData
     */
    std::map<uint8_t, std::vector<MPSData>> getGroupedConfigData(
        uint8_t configMask = 0xFF, uint8_t shift = 0);

  protected:
    phosphor::i2c::I2C i2cInterface;
    std::unique_ptr<MPSImageParser> parser;
    std::unique_ptr<MPSConfig> configuration;
};

} // namespace phosphor::software::VR
