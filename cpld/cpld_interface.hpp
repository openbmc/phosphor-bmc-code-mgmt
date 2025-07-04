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
    CPLDInterface(sdbusplus::async::context& ctx, const std::string& chipname,
                  uint16_t bus, uint8_t address) :
        ctx(ctx), chipname(chipname), bus(bus), address(address)
    {}

    virtual ~CPLDInterface() = default;
    CPLDInterface(const CPLDInterface&) = delete;
    CPLDInterface& operator=(const CPLDInterface&) = delete;
    CPLDInterface(CPLDInterface&&) = delete;
    CPLDInterface& operator=(CPLDInterface&&) = delete;

    virtual sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progress = nullptr) = 0;

    virtual sdbusplus::async::task<bool> getVersion(std::string& version) = 0;

  protected:
    sdbusplus::async::context& ctx;
    std::string chipname;
    uint16_t bus;
    uint8_t address;
};

class CPLDFactory
{
  public:
    using Creator = std::function<std::unique_ptr<CPLDInterface>(
        sdbusplus::async::context& ctx, const std::string& chipType,
        uint16_t bus, uint8_t address)>;
    using ConfigProvider = std::function<std::vector<std::string>()>;

    static CPLDFactory& instance();

    void registerCPLD(const std::string& chipType, Creator creator);

    std::unique_ptr<CPLDInterface> create(
        const std::string& chipType, sdbusplus::async::context& ctx,
        const std::string& chipName, uint16_t bus, uint8_t address) const;

    std::vector<std::string> getConfigs();

  private:
    std::unordered_map<std::string, Creator> creators;
};

} // namespace phosphor::software::cpld
