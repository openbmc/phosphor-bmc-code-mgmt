#pragma once
#include "lattice_base_cpld.hpp"

namespace phosphor::software::cpld
{
enum class xo5Cmd : uint8_t
{
    sectorErase = 0xd8,
    pageProgram = 0x02,
    pageRead = 0x0b,
    readUsercode = 0xc0,
    noop = 0xff,
    noop0 = 0x00,
    readDeviceID = 0xe0,
    readSoftIpId = 0xe6,
    readStatusReg = 0x3c,
    checkBusyStatus = 0xf0,
    enablex = 0x74,
    disable = 0x26,
    initAddress = 0x46,
    erase = 0x0e,
    programIncr = 0x82,
    readIncr = 0x6a,
    programDone = 0x5e,
    refresh = 0x79,
    calcHash = 0x7c,
    readHash = 0xe5,
    controlCmdCrc = 0xfd,
    lock = 0xa2
};

enum class xo5Status : uint8_t
{
    ready = 0x00,
    notReady = 0xff
};

struct xo5Cfg
{
    static constexpr size_t pageSize = 256;
    static constexpr size_t pagesPerBlock = 256;
    static constexpr size_t blocksPerCfg = 11;

    static constexpr size_t incrDataSize = 128;
    static constexpr size_t retryMax = 3;
    static constexpr uint8_t headerIdx = 0;
    static constexpr uint8_t cfg0Idx = 1;
    static constexpr uint8_t cfg1Idx = 2;
    static constexpr uint8_t ufm8Idx = 15;
};

class LatticeXO5BaseCPLD : public LatticeBaseCPLD
{
  public:
    LatticeXO5BaseCPLD(sdbusplus::async::context& ctx, uint16_t bus,
                       uint8_t address, const std::string& chip,
                       const std::string& target,
                       std::chrono::milliseconds interval, bool debugMode);
    ~LatticeXO5BaseCPLD() override = default;
    LatticeXO5BaseCPLD(const LatticeXO5BaseCPLD&) = delete;
    LatticeXO5BaseCPLD& operator=(const LatticeXO5BaseCPLD&) = delete;
    LatticeXO5BaseCPLD(LatticeXO5BaseCPLD&&) = delete;
    LatticeXO5BaseCPLD& operator=(LatticeXO5BaseCPLD&&) = delete;

  protected:
    static constexpr std::chrono::milliseconds readyTimeout{1000};
    static constexpr std::chrono::milliseconds eraseTimeout{20000};

    std::chrono::milliseconds pollInterval;

    sdbusplus::async::task<bool> waitUntilReady(
        std::chrono::milliseconds timeout);

    sdbusplus::async::task<bool> doUpdate() override;

    virtual sdbusplus::async::task<bool> checkDeviceReady() = 0;

    virtual uint8_t getCfgIdx(std::string_view target) const = 0;

    virtual sdbusplus::async::task<bool> eraseCfg(
        std::optional<uint8_t> setIdx = std::nullopt) = 0;

    virtual sdbusplus::async::task<bool> programCfg(
        std::optional<uint8_t> setIdx = std::nullopt,
        const std::vector<uint8_t>* customData = nullptr) = 0;

    virtual sdbusplus::async::task<bool> verifyCfg() = 0;

    static uint32_t extractUint32(const std::vector<uint8_t>& data,
                                  size_t offset, bool isBigEndian);
};

} // namespace phosphor::software::cpld
