#include "sync_manager.hpp"

#include <signal.h>
#include <sys/types.h>

#include <gtest/gtest.h>

namespace
{
constexpr pid_t childPid = 42;
int childStatus = 0;
} // namespace

extern "C" pid_t __wrap_fork()
{
    return childPid;
}

extern "C" pid_t __wrap_waitpid(pid_t pid, int* status, int)
{
    EXPECT_EQ(pid, childPid);
    *status = childStatus;
    return childPid;
}

namespace phosphor::software::manager
{

TEST(SyncManagerTest, ReturnsSuccessWhenRsyncExitsSuccessfully)
{
    childStatus = 0;

    EXPECT_EQ(Sync::processEntry(0, "/tmp/source"), 0);
}

TEST(SyncManagerTest, ReturnsFailureWhenRsyncExitsWithFailure)
{
    childStatus = 1 << 8;

    EXPECT_EQ(Sync::processEntry(0, "/tmp/source"), -1);
}

TEST(SyncManagerTest, ReturnsFailureWhenRsyncIsTerminatedBySignal)
{
    childStatus = SIGTERM;

    EXPECT_EQ(Sync::processEntry(0, "/tmp/source"), -1);
}

} // namespace phosphor::software::manager