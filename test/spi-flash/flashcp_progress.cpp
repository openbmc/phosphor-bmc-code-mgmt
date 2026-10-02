
#include "../../spi-flash/flashcp_progress.hpp"

#include <optional>

#include <gtest/gtest.h>

TEST(FlashcpProgress, TestErasingProgress)
{
    EXPECT_EQ(parseFlashcpProgress("Erasing block (0%)"), 30U);
    EXPECT_EQ(parseFlashcpProgress("Erasing block (50%)"), 37U);
    EXPECT_EQ(parseFlashcpProgress("Erasing block (100%)"), 45U);
}

TEST(FlashcpProgress, TestWritingProgress)
{
    EXPECT_EQ(parseFlashcpProgress("Writing kb (0%)"), 45U);
    EXPECT_EQ(parseFlashcpProgress("Writing kb (50%)"), 62U);
    EXPECT_EQ(parseFlashcpProgress("Writing kb (100%)"), 80U);
}

TEST(FlashcpProgress, TestVerifyingProgress)
{
    EXPECT_EQ(parseFlashcpProgress("Verifying kb (0%)"), 80U);
    EXPECT_EQ(parseFlashcpProgress("Verifying kb (50%)"), 85U);
    EXPECT_EQ(parseFlashcpProgress("Verifying kb (100%)"), 90U);
}

TEST(FlashcpProgress, TestInvalidPercentage)
{
    EXPECT_EQ(parseFlashcpProgress("Writing kb (101%)"), std::nullopt);
    EXPECT_EQ(parseFlashcpProgress("Writing kb (-1%)"), std::nullopt);
    EXPECT_EQ(parseFlashcpProgress("Writing kb (abc%)"), std::nullopt);
}

TEST(FlashcpProgress, TestInvalidFormat)
{
    EXPECT_EQ(parseFlashcpProgress("Writing kb"), std::nullopt);
    EXPECT_EQ(parseFlashcpProgress("Writing kb (50)"), std::nullopt);
    EXPECT_EQ(parseFlashcpProgress("Writing kb 50%"), std::nullopt);
    EXPECT_EQ(parseFlashcpProgress("Unknown (50%)"), std::nullopt);
    EXPECT_EQ(parseFlashcpProgress(""), std::nullopt);
}
