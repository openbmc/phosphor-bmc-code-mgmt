#pragma once
#include <gpiod.hpp>

#include <fstream>
#include <map>
#include <string>

std::vector<std::unique_ptr<::gpiod::line_bulk>> requestMuxGPIOs(
    const std::vector<std::string>& gpioLines,
    const std::vector<bool>& gpioPolarities, bool inverted);

class GPIOGroup
{
  public:
    GPIOGroup(std::vector<std::string> lines, std::vector<bool> polarities);

    ~GPIOGroup();

    bool muxToBMC();
    bool muxToHost();
    bool muxToDevice()
    {
        return muxToHost();
    }

    void releaseAll();

    GPIOGroup(const GPIOGroup&) = delete;
    GPIOGroup& operator=(const GPIOGroup&) = delete;
    GPIOGroup(GPIOGroup&&) noexcept;
    GPIOGroup& operator=(GPIOGroup&&) noexcept;

  private:
    bool mux(bool inverted);

    std::vector<std::string> lines_;
    std::vector<bool> polarities_;
    std::vector<std::unique_ptr<::gpiod::line_bulk>> activeBulks_;
};

class MuxGuard
{
  public:
    explicit MuxGuard(GPIOGroup& group);
    ~MuxGuard();

    MuxGuard(const MuxGuard&) = delete;
    MuxGuard& operator=(const MuxGuard&) = delete;

  private:
    GPIOGroup& group_;
};
