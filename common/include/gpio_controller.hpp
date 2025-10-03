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

    std::vector<std::string> lines_;
    std::vector<bool> polarities_;
    std::vector<std::unique_ptr<::gpiod::line_bulk>> activeBulks_;
};

/*
 * @class MuxGuard
 * @brief A RAII guard to mux GPIO lines to BMC on construction
 * and back to device on destruction
 */
class MuxGuard
{
  public:
    explicit MuxGuard(GPIOGroup& group);
    ~MuxGuard();

    MuxGuard(const MuxGuard&) = delete;
    MuxGuard& operator=(const MuxGuard&) = delete;

    MuxGuard(MuxGuard&& /*other*/) = delete;
    MuxGuard& operator=(MuxGuard&& /*other*/) = delete;

    bool ok() const
    {
      return ok_;
    }

  private:
    GPIOGroup& group_;
    bool ok_{false};
};
