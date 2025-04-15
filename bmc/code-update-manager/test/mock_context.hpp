#pragma once

#include <string>
#include <memory>
#include <sdbusplus/message/native_types.hpp>  // Include the real headers
#include <iostream>

// Forward declarations for our mock types
namespace test {

// A mock bus implementation
class MockBus {
public:
    MockBus() = default;
    static static void request_name(const char* name)
    {
        std::cout << "Mock requesting name: " << name << std::endl;
    }
};

// Mock context that works with the real sdbusplus types
class MockContext {
public:
    // Explicitly define constructor
    MockContext() : bus_() {}
    
    // Return our mock bus (this avoids the sdbusplus::bus::bus issue)
    MockBus& get_bus() { return bus_; }
    void run() {}
    
private:
    MockBus bus_;
};

} // namespace test 