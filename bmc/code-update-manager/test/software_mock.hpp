#pragma once

#include <memory>
#include <string>

namespace sdbusplus_mock {
namespace async {
class mock_context {
public:
    mock_context() = default;
    mock_context(void* /*unused*/ /*unused*/) {}

    // Mock bus interface
    class bus {
    public:
      void request_name(const char* /*unused*/ /*unused*/) {}
    };
    
    bus& get_bus() { return bus_; }
    void run() {}
    
private:
    bus bus_;
};
}

namespace xyz::openbmc_project::Software::server
{
namespace Activation 
{
enum class Activations
{
    NotReady,
    Ready,
    Activating,
    Active,
    Failed
};
}

namespace ApplyTime
{
enum class RequestedApplyTimes
{
    OnReset,
    Immediate
};
}
}

} // namespace sdbusplus_mock

namespace phosphor
{
namespace software
{

class MockSoftware;

class MockSoftware
{
  public:
    MockSoftware(sdbusplus_mock::async::mock_context& /*ctx*/, const std::string& id) :
        id_(id), 
        activationState_(sdbusplus_mock::xyz::openbmc_project::Software::server::Activation::Activations::NotReady),
        progress_(0)
    {
        softwareActivationProgress = std::make_shared<ActivationProgress>(*this);
    }
    
    virtual ~MockSoftware() = default;
    
    virtual void setActivation(sdbusplus_mock::xyz::openbmc_project::Software::server::Activation::Activations status) {
        activationState_ = status;
    }
    
    virtual sdbusplus_mock::xyz::openbmc_project::Software::server::Activation::Activations getActivationState() const {
        return activationState_;
    }
    
    virtual uint8_t getProgress() const {
        return progress_;
    }
    
    virtual std::string getVersion() const { return "1.0"; }
    virtual std::string getPurpose() const { return "test"; }
    virtual std::string getId() const { return id_; }
    
    class ActivationProgress
    {
    public:
        explicit ActivationProgress(MockSoftware& parent) : 
            parent_(parent) {}
        
        void setProgress(uint8_t progress) {
            parent_.progress_ = progress;
        }
        
    private:
        MockSoftware& parent_;
    };
    
    std::shared_ptr<ActivationProgress> softwareActivationProgress;
    
  private:
    std::string id_;
    sdbusplus_mock::xyz::openbmc_project::Software::server::Activation::Activations activationState_;
    uint8_t progress_;
};

} // namespace software
} // namespace phosphor