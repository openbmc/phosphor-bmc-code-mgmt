#include "common/include/NotifyWatch.hpp"

#include <cerrno>

#include <gtest/gtest.h>

namespace
{

struct TestInstance
{};

using TestNotifyWatch = phosphor::notify::watch::NotifyWatch<TestInstance>;

TEST(NotifyWatchTest, IdentifiesTransientReadErrors)
{
    EXPECT_TRUE(TestNotifyWatch::isTransientReadError(EAGAIN));
    EXPECT_TRUE(TestNotifyWatch::isTransientReadError(EINTR));

#ifdef EWOULDBLOCK
    EXPECT_TRUE(TestNotifyWatch::isTransientReadError(EWOULDBLOCK));
#endif
}

TEST(NotifyWatchTest, IdentifiesFatalReadErrors)
{
    EXPECT_FALSE(TestNotifyWatch::isTransientReadError(EIO));
}

} // namespace
