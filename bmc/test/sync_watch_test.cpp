#include "../sync_watch.hpp"

#include <systemd/sd-event.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

namespace phosphor::software::manager
{
namespace
{

class SyncWatchTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        char directoryTemplate[] = "/tmp/sync-watch-testXXXXXX";
        auto directory = mkdtemp(directoryTemplate);
        ASSERT_NE(directory, nullptr);
        directoryPath = directory;
        firstFile = directoryPath / "first";
        secondFile = directoryPath / "second";
        syncListPath = directoryPath / "synclist";

        std::ofstream(firstFile) << "first";
        std::ofstream(secondFile) << "second";
        std::ofstream syncList(syncListPath);
        syncList << firstFile.string() << '\n' << secondFile.string() << '\n';
        ASSERT_TRUE(syncList.good());
        ASSERT_EQ(sd_event_new(&event), 0);
    }

    void TearDown() override
    {
        if (event != nullptr)
        {
            sd_event_unref(event);
        }
        std::error_code error;
        fs::remove_all(directoryPath, error);
    }

    fs::path directoryPath;
    fs::path firstFile;
    fs::path secondFile;
    fs::path syncListPath;
    sd_event* event = nullptr;
};

TEST_F(SyncWatchTest, ProcessesEventsAfterIgnoredWatchInSameBatch)
{
    bool secondFileSynced = false;
    SyncWatch watch(
        *event,
        [this, &secondFileSynced](int, fs::path& path) {
            if (path == secondFile)
            {
                secondFileSynced = true;
                sd_event_exit(event, 0);
            }
            return 0;
        },
        syncListPath);

    std::error_code error;
    ASSERT_TRUE(fs::remove(firstFile, error));
    std::ofstream(secondFile, std::ios::app) << " changed";

    for (int attempt = 0; attempt < 10 && !secondFileSynced; ++attempt)
    {
        ASSERT_GE(sd_event_run(event, 100000), 0);
    }

    EXPECT_TRUE(secondFileSynced);
}

} // namespace
} // namespace phosphor::software::manager