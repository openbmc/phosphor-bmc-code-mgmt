#pragma once

#include <gpiod.hpp>

#include <string>

std::vector<std::unique_ptr<::gpiod::line_bulk>> requestMuxGPIOs(
    const std::vector<std::string>& gpioLines,
    const std::vector<bool>& gpioPolarities, bool inverted);

/*
 * @class GPIOGroup
 * @brief A group of GPIO lines to be muxed together
 */
class GPIOGroup
{
  public:
    GPIOGroup(std::vector<std::string> lines, std::vector<bool> polarities);

    ~GPIOGroup();

    // GPIOs set to route mux to the BMC
    bool muxToBMC();
    // GPIOs set to the opposite values to route mux to the external device
    bool muxToDevice();

    void releaseAll();

    GPIOGroup(const GPIOGroup&) = delete;
    GPIOGroup& operator=(const GPIOGroup&) = delete;
    GPIOGroup(GPIOGroup&& /*other*/) noexcept;
    GPIOGroup& operator=(GPIOGroup&& /*other*/) noexcept;

  private:
    bool mux(bool inverted);

    std::vector<std::string> lines;
    std::vector<bool> polarities;
    std::vector<std::unique_ptr<::gpiod::line_bulk>> activeBulks;
};

/*
 * @class ScopedBmcMux
 * @brief A RAII wrapper to mux GPIO lines to BMC on construction
 * and back to device on destruction
 */
class ScopedBmcMux
{
  public:
    explicit ScopedBmcMux(GPIOGroup& group);
    ~ScopedBmcMux();

    ScopedBmcMux(const ScopedBmcMux&) = delete;
    ScopedBmcMux& operator=(const ScopedBmcMux&) = delete;

    ScopedBmcMux(ScopedBmcMux&& /*other*/) = delete;
    ScopedBmcMux& operator=(ScopedBmcMux&& /*other*/) = delete;

  private:
    GPIOGroup& gpioGroup;
};
