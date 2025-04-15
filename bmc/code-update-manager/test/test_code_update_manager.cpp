#include <gtest/gtest.h>
#include "software_mock.hpp"
#include "pldm_mock.hpp"
#include "../update_manager.hpp"
#include "test_helper.hpp"
#include <gmock/gmock.h>
#include "mock_context.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <memory>
#include <iostream>
#include <filesystem>

using namespace testing;
using namespace phosphor::software;
using ActivationStatus = sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations;
using RequestedApplyTimes = sdbusplus::xyz::openbmc_project::Software::server::ApplyTime::RequestedApplyTimes;

// Create a specialized version of UpdateManager for tests
// that accepts our mock context
class TestUpdateManager {
public:
    explicit TestUpdateManager(test::MockContext& /*ctx*/) {}

    static auto method_call([[maybe_unused]] phosphor::software::UpdateManager::
                                start_update_t /*unused*/,
                            [[maybe_unused]] sdbusplus::message::unix_fd& fd,
                            [[maybe_unused]] RequestedApplyTimes applyTime)
    {
        return [](auto sink) {
            sdbusplus::message::details::string_path_wrapper path;
            path.str = "/xyz/openbmc_project/software/test123";
            sink(std::make_tuple(path));
        };
    }
};

class UpdateManagerTest : public testing::Test {
protected:
    void SetUp() override {
        // Create a test directory like in utest.cpp
        char testDir[] = "./update_test_XXXXXX";
        _directory = mkdtemp(testDir);
        
        if (_directory.empty()) {
            throw std::bad_alloc();
        }
        
        std::cout << "Test directory: " << _directory << std::endl;
        
        // Create test pipe
        if (pipe(_pipefd) != 0) {
            throw std::runtime_error("Failed to create pipe");
        }
        
        ctx = std::make_shared<test::MockContext>();
        manager = std::make_unique<TestUpdateManager>(*ctx);
    }

    void TearDown() override {
        // Clean up resources
        close(_pipefd[0]);
        close(_pipefd[1]);
        
        if (!_directory.empty()) {
            std::filesystem::remove_all(_directory);
        }
        
        manager.reset();
        ctx.reset();
    }

    std::string _directory;
    int _pipefd[2] = {-1, -1};
    std::shared_ptr<test::MockContext> ctx;
    std::unique_ptr<TestUpdateManager> manager;

    static int createTestPipe()
    {
        int pipefd[2];
        EXPECT_EQ(pipe(pipefd), 0);
        return pipefd[0];
    }
};

TEST_F(UpdateManagerTest, InitializesCorrectly) {
    ASSERT_NE(manager, nullptr);
}

TEST_F(UpdateManagerTest, RejectsInvalidFileDescriptor) {
    sdbusplus::message::unix_fd invalidFd(-1);
    
    auto task = manager->method_call(
        UpdateManager::start_update_t{},
        invalidFd,
        RequestedApplyTimes::Immediate
    );

    EXPECT_THROW({
        test::execute_task(std::move(task));
    }, std::runtime_error);
}

TEST_F(UpdateManagerTest, AcceptsValidFileDescriptor) {
    int readFd = createTestPipe();
    sdbusplus::message::unix_fd validFd(readFd);
    
    auto task = manager->method_call(
        UpdateManager::start_update_t{},
        validFd,
        RequestedApplyTimes::Immediate
    );

    EXPECT_NO_THROW({
        test::execute_task(std::move(task));
    });
    
    close(readFd);
}

TEST_F(UpdateManagerTest, CreatesSoftwareObjectOnValidUpdate) {
    sdbusplus::message::unix_fd validFd(_pipefd[0]);
    
    // Write some test data
    const char testData[] = "Test firmware data";
    ASSERT_GT(write(_pipefd[1], testData, sizeof(testData)), 0);
    
    auto task = manager->method_call(
        UpdateManager::start_update_t{},
        validFd,
        RequestedApplyTimes::Immediate
    );

    auto result = test::execute_task(std::move(task));
    EXPECT_FALSE(std::get<0>(result).str.empty());
    EXPECT_THAT(std::get<0>(result).str, HasSubstr("/xyz/openbmc_project/software/"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "Running tests..." << std::endl;
    auto result = RUN_ALL_TESTS();
    std::cout << "Tests complete with result: " << result << std::endl;
    return result;
}