#include "vr.hpp"

#include <sdbusplus/async.hpp>

namespace phosphor::software::VR
{

class DummyDevice : public VoltageRegulator
{
  public:
    DummyDevice(sdbusplus::async::context& ctx);

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> reset() final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    bool forcedUpdateAllowed() final;
};

} // namespace phosphor::software::VR
