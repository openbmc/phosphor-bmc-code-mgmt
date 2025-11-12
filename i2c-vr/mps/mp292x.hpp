#pragma once

#include "mp994x.hpp"

namespace phosphor::software::VR
{

constexpr auto mp292xConfigIdCmd = 0xA9;

/**
 * @class MP292X
 * @brief Derived class for MP292X voltage regulator
 *
 * The firmware update process for MP292X is identical to MP994X.
 * The only difference is the Config ID register, which uses 0xA9
 * instead of 0xAF. All other steps, such as unlocking write protect,
 * programming registers, restoring from NVM, and CRC checks, remain
 * the same as MP994X.
 */
class MP292X : public MP994X
{
  public:
    using MP994X::MP994X;

  protected:
    MP994XCmd getConfigIdCmd() const final
    {
        return static_cast<MP994XCmd>(mp292xConfigIdCmd);
    }
};

} // namespace phosphor::software::VR
