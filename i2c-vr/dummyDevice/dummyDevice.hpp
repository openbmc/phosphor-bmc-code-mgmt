#include "vr.hpp"

#include <sdbusplus/async.hpp>

namespace SDBusAsync = sdbusplus::async;

namespace phosphor::software::VR
{

class DummyDevice : public VoltageRegulator
{
  public:
    DummyDevice(SDBusAsync::context& ctx);

    SDBusAsync::task<bool> verifyImage(const uint8_t* image,
                                       size_t imageSize) final;
    SDBusAsync::task<bool> updateFirmware(bool force) final;
    SDBusAsync::task<bool> reset() final;
    bool getCRC(uint32_t* checksum) final;
};

} // namespace phosphor::software::VR
