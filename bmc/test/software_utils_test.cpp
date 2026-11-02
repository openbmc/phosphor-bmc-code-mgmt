#include "software_utils.hpp"

#include <signal.h>
#include <sys/types.h>

#include <cerrno>

#include <gtest/gtest.h>

namespace
{
constexpr pid_t childPid = 42;

struct WaitpidMock
{
    int status = 0;
    int failErrno = 0;
    int failCount = 0;
    int calls = 0;
};

WaitpidMock& mock()
{
    static WaitpidMock m;
    return m;
}
} // namespace

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
extern "C" pid_t __wrap_fork()
{
    return childPid;
}

// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
extern "C" pid_t __wrap_waitpid(pid_t pid, int* status, int /*unused*/)
{
    EXPECT_EQ(pid, childPid);
    auto& m = mock();
    m.calls++;
    if (m.failCount > 0)
    {
        m.failCount--;
        errno = m.failErrno;
        return -1;
    }
    *status = m.status;
    return childPid;
}

namespace phosphor::software::utils
{

class UnTarTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        mock() = WaitpidMock{};
    }
};

TEST_F(UnTarTest, ReturnsTrueWhenTarExitsSuccessfully)
{
    EXPECT_TRUE(unTar(0, "/tmp/extract"));
}

TEST_F(UnTarTest, ReturnsFalseWhenTarExitsWithFailure)
{
    mock().status = 2 << 8;

    EXPECT_FALSE(unTar(0, "/tmp/extract"));
}

TEST_F(UnTarTest, ReturnsFalseWhenTarIsTerminatedBySignal)
{
    mock().status = SIGTERM;

    EXPECT_FALSE(unTar(0, "/tmp/extract"));
}

TEST_F(UnTarTest, ReturnsFalseWhenWaitpidFails)
{
    mock().failCount = 1;
    mock().failErrno = ECHILD;

    EXPECT_FALSE(unTar(0, "/tmp/extract"));
    EXPECT_EQ(mock().calls, 1);
}

TEST_F(UnTarTest, RetriesWaitpidOnEintr)
{
    mock().failCount = 2;
    mock().failErrno = EINTR;

    EXPECT_TRUE(unTar(0, "/tmp/extract"));
    EXPECT_EQ(mock().calls, 3);
}

} // namespace phosphor::software::utils
