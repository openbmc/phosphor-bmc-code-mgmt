#pragma once

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace phosphor::software::cpld
{

class CPLDInterface
{
  public:
    explicit CPLDInterface() {}
    virtual ~CPLDInterface() = default;

    CPLDInterface(const CPLDInterface&) = delete;
    CPLDInterface& operator=(const CPLDInterface&) = delete;
    CPLDInterface(CPLDInterface&&) = delete;
    CPLDInterface& operator=(CPLDInterface&&) = delete;

    virtual sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize) = 0;
};

class CPLDFactory
{
  public:
    using Creator = std::function<std::unique_ptr<CPLDInterface>(
        const std::string& chipName, uint16_t bus, uint8_t address)>;
    using ConfigProvider = std::function<std::vector<std::string>()>;

    static CPLDFactory& instance();

    void registerCPLD(const std::string& vendorName, Creator creator);

    std::unique_ptr<CPLDInterface> create(const std::string& vendorName,
                                          const std::string& chipName,
                                          uint16_t bus, uint8_t address) const;

    std::vector<std::string> getConfigs();

  private:
    std::unordered_map<std::string, Creator> creators;
};

} // namespace phosphor::software::cpld
