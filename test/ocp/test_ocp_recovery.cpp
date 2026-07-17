#include "mock_transport.hpp"

#include <ocp/ocp_recovery.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace
{

std::vector<uint8_t> validProtCap(uint16_t caps)
{
    return {15,
            'O',
            'C',
            'P',
            ' ',
            'R',
            'E',
            'C',
            'V',
            1,
            0,
            static_cast<uint8_t>(caps & 0xFF),
            static_cast<uint8_t>(caps >> 8),
            2, /* num_cms */
            10 /* max_resp_time */,
            0 /* heartbeat */};
}

std::vector<uint8_t> deviceStatusResponse(uint8_t status)
{
    return {4, status, 0x00, 0x00, 0x00};
}

std::vector<uint8_t> recoveryStatusResponse(uint8_t statusByte)
{
    return {2, statusByte, 0x00};
}

std::vector<uint8_t> indirectStatusResponse(uint8_t statusByte)
{
    return {6, statusByte, 0x00, 0x40, 0x00, 0x00, 0x00};
}

class OCPRecoveryTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ASSERT_EQ(ocp_recov_init_transport(&ctx, mockTransfer, &mock, 0x6F), 0);
    }

    MockTransport mock;
    struct ocp_recov_ctx ctx = {};
};

TEST_F(OCPRecoveryTest, ForceRecoveryEncoding)
{
    ASSERT_EQ(ocp_recov_force_recovery(&ctx), 0);

    const std::vector<uint8_t> expected{0x25, 0x03, 0x01, 0x0F, 0x01};
    ASSERT_EQ(mock.writes.size(), 1U);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, RecoveryCtrlEncoding)
{
    ASSERT_EQ(ocp_recov_recovery_ctrl(&ctx, 1,
                                      OCP_RECOV_RECOVERY_IMAGE_FROM_CMS,
                                      OCP_RECOV_RECOVERY_ACTIVATE),
              0);

    const std::vector<uint8_t> expected{0x26, 0x03, 0x01, 0x01, 0x0F};
    ASSERT_EQ(mock.writes.size(), 1U);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, IndirectCtrlEncoding)
{
    ASSERT_EQ(ocp_recov_indirect_ctrl(&ctx, 0, 0), 0);
    ASSERT_EQ(ocp_recov_indirect_ctrl(&ctx, 2, 0x12345678), 0);

    const std::vector<uint8_t> zeroOffset{0x29, 0x06, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> leOffset{0x29, 0x06, 0x02, 0x00,
                                        0x78, 0x56, 0x34, 0x12};
    ASSERT_EQ(mock.writes.size(), 2U);
    EXPECT_EQ(mock.writes[0], zeroOffset);
    EXPECT_EQ(mock.writes[1], leOffset);
}

TEST_F(OCPRecoveryTest, IndirectDataFraming)
{
    const uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(ocp_recov_indirect_data_write(&ctx, data, sizeof(data)), 0);

    const std::vector<uint8_t> expected{0x2B, 0x04, 0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(mock.writes.size(), 1U);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, IndirectDataRejectsOversizedChunk)
{
    std::vector<uint8_t> data(OCP_RECOV_CHUNK_SIZE_MAX + 1, 0xAA);
    EXPECT_EQ(ocp_recov_indirect_data_write(&ctx, data.data(), data.size()),
              -EINVAL);

    ctx.chunk_size = 32;
    std::vector<uint8_t> chunk(33, 0xBB);
    EXPECT_EQ(ocp_recov_indirect_data_write(&ctx, chunk.data(), chunk.size()),
              -EINVAL);
}

TEST_F(OCPRecoveryTest, ProtCapDecode)
{
    mock.staticResponses[OCP_RECOV_CMD_PROT_CAP] =
        validProtCap(OCP_RECOV_PROT_CAP_INDIRECT_CTRL);

    struct ocp_recov_prot_cap cap = {};
    ASSERT_EQ(ocp_recov_get_prot_cap(&ctx, &cap), 0);

    EXPECT_STREQ(cap.magic, "OCP RECV");
    EXPECT_EQ(cap.major, 1);
    EXPECT_EQ(cap.minor, 0);
    EXPECT_EQ(cap.caps, OCP_RECOV_PROT_CAP_INDIRECT_CTRL);
    EXPECT_EQ(cap.num_cms, 2);
    EXPECT_EQ(cap.max_resp_time, 10);
}

TEST_F(OCPRecoveryTest, ProtCapBadMagic)
{
    auto resp = validProtCap(0);
    resp[1] = 'X';
    mock.staticResponses[OCP_RECOV_CMD_PROT_CAP] = resp;

    struct ocp_recov_prot_cap cap = {};
    EXPECT_EQ(ocp_recov_get_prot_cap(&ctx, &cap), -EPROTO);
}

TEST_F(OCPRecoveryTest, CheckCaps)
{
    struct ocp_recov_prot_cap cap = {};

    cap.caps = 0;
    EXPECT_EQ(ocp_recov_check_caps(&cap), 0);

    cap.caps = OCP_RECOV_PROT_CAP_INDIRECT_CTRL;
    EXPECT_EQ(ocp_recov_check_caps(&cap), 0);

    cap.caps = OCP_RECOV_PROT_CAP_INDIRECT_CTRL |
               OCP_RECOV_PROT_CAP_INDIRECT_FIFO;
    EXPECT_EQ(ocp_recov_check_caps(&cap), 0);

    cap.caps = OCP_RECOV_PROT_CAP_INDIRECT_FIFO;
    EXPECT_EQ(ocp_recov_check_caps(&cap), -ENOTSUP);
}

TEST_F(OCPRecoveryTest, DeviceStatusDecode)
{
    mock.staticResponses[OCP_RECOV_CMD_DEVICE_STATUS] = {
        4, OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE, 0x02, 0x34, 0x12};

    ocp_recov_device_status status = OCP_RECOV_DEVICE_STATUS_PENDING;
    uint8_t protErr = 0;
    uint16_t reason = 0;
    ASSERT_EQ(ocp_recov_get_device_status(&ctx, &status, &protErr, &reason), 0);

    EXPECT_EQ(status, OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE);
    EXPECT_EQ(protErr, 0x02);
    EXPECT_EQ(reason, 0x1234);
}

TEST_F(OCPRecoveryTest, RecoveryStatusDecode)
{
    mock.staticResponses[OCP_RECOV_CMD_RECOVERY_STATUS] = {2, 0x13, 0xAB};

    ocp_recov_recovery_status status =
        OCP_RECOV_RECOVERY_STATUS_NOT_IN_RECOVERY;
    uint8_t index = 0;
    uint8_t vendor = 0;
    ASSERT_EQ(ocp_recov_get_recovery_status(&ctx, &status, &index, &vendor), 0);

    EXPECT_EQ(status, OCP_RECOV_RECOVERY_STATUS_SUCCESS);
    EXPECT_EQ(index, 0x01);
    EXPECT_EQ(vendor, 0xAB);
}

TEST_F(OCPRecoveryTest, IndirectStatusAckBit)
{
    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_STATUS] =
        indirectStatusResponse(0x04);

    uint8_t status = 0;
    bool ack = false;
    uint32_t size = 0;
    ASSERT_EQ(ocp_recov_indirect_status(&ctx, &status, &ack, &size), 0);
    EXPECT_TRUE(ack);
    EXPECT_EQ(size, 0x40U);

    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_STATUS] =
        indirectStatusResponse(0x01);
    ASSERT_EQ(ocp_recov_indirect_status(&ctx, &status, &ack, nullptr), 0);
    EXPECT_FALSE(ack);
}

TEST_F(OCPRecoveryTest, IndirectDataRead)
{
    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_DATA] = {4, 'L', 'O', 'G', 'S'};

    uint8_t buf[16] = {};
    size_t out = 0;
    ASSERT_EQ(ocp_recov_indirect_data_read(&ctx, buf, sizeof(buf), &out), 0);
    EXPECT_EQ(out, 4U);
    EXPECT_EQ(std::memcmp(buf, "LOGS", 4), 0);
}

TEST_F(OCPRecoveryTest, RecoverHappyPathSequenceAndChunking)
{
    mock.staticResponses[OCP_RECOV_CMD_PROT_CAP] =
        validProtCap(OCP_RECOV_PROT_CAP_INDIRECT_CTRL);
    mock.staticResponses[OCP_RECOV_CMD_DEVICE_STATUS] =
        deviceStatusResponse(OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE);
    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_STATUS] =
        indirectStatusResponse(0x04);
    mock.staticResponses[OCP_RECOV_CMD_RECOVERY_STATUS] =
        recoveryStatusResponse(OCP_RECOV_RECOVERY_STATUS_SUCCESS);

    std::vector<uint8_t> image(1000, 0x5A);

    size_t lastWritten = 0;
    auto progress = [](void* user, size_t written, size_t) {
        *static_cast<size_t*>(user) = written;
    };

    ASSERT_EQ(ocp_recov_recover(&ctx, image.data(), image.size(), progress,
                                &lastWritten),
              0);
    EXPECT_EQ(lastWritten, image.size());

    const std::vector<uint8_t> expectedSequence{
        OCP_RECOV_CMD_PROT_CAP,      OCP_RECOV_CMD_DEVICE_STATUS,
        OCP_RECOV_CMD_RECOVERY_CTRL, OCP_RECOV_CMD_INDIRECT_CTRL,
        OCP_RECOV_CMD_INDIRECT_DATA, OCP_RECOV_CMD_INDIRECT_STATUS,
        OCP_RECOV_CMD_INDIRECT_DATA, OCP_RECOV_CMD_INDIRECT_STATUS,
        OCP_RECOV_CMD_INDIRECT_DATA, OCP_RECOV_CMD_INDIRECT_STATUS,
        OCP_RECOV_CMD_INDIRECT_DATA, OCP_RECOV_CMD_INDIRECT_STATUS,
        OCP_RECOV_CMD_RECOVERY_CTRL, OCP_RECOV_CMD_RECOVERY_STATUS,
    };
    EXPECT_EQ(mock.commandSequence(), expectedSequence);

    /* 1000 bytes -> 252 + 252 + 252 + 244 */
    std::vector<size_t> chunkSizes;
    for (const auto& w : mock.writes)
    {
        if (w[0] == OCP_RECOV_CMD_INDIRECT_DATA)
        {
            chunkSizes.push_back(w[1]);
        }
    }
    const std::vector<size_t> expectedChunks{252, 252, 252, 244};
    EXPECT_EQ(chunkSizes, expectedChunks);

    /* activation is the last RECOVERY_CTRL write */
    const std::vector<uint8_t> activate{0x26, 0x03, OCP_RECOV_CMS_DEFAULT,
                                        OCP_RECOV_RECOVERY_IMAGE_FROM_CMS,
                                        OCP_RECOV_RECOVERY_ACTIVATE};
    EXPECT_EQ(mock.writes[mock.writes.size() - 2], activate);
}

TEST_F(OCPRecoveryTest, RecoverToleratesMissingProtCap)
{
    mock.commandErrors[OCP_RECOV_CMD_PROT_CAP] = -EIO;
    mock.staticResponses[OCP_RECOV_CMD_DEVICE_STATUS] =
        deviceStatusResponse(OCP_RECOV_DEVICE_STATUS_RECOVERY_MODE);
    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_STATUS] =
        indirectStatusResponse(0x04);
    mock.staticResponses[OCP_RECOV_CMD_RECOVERY_STATUS] =
        recoveryStatusResponse(OCP_RECOV_RECOVERY_STATUS_SUCCESS);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(
        ocp_recov_recover(&ctx, image.data(), image.size(), nullptr, nullptr),
        0);
}

TEST_F(OCPRecoveryTest, RecoverRejectsFifoOnlyDevice)
{
    mock.staticResponses[OCP_RECOV_CMD_PROT_CAP] =
        validProtCap(OCP_RECOV_PROT_CAP_INDIRECT_FIFO);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(
        ocp_recov_recover(&ctx, image.data(), image.size(), nullptr, nullptr),
        -ENOTSUP);
}

TEST_F(OCPRecoveryTest, WriteImageAckTimeout)
{
    /* device never acknowledges the chunk */
    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_STATUS] =
        indirectStatusResponse(0x00);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(ocp_recov_write_image(&ctx, image.data(), image.size(), nullptr,
                                    nullptr),
              -ETIMEDOUT);
}

TEST_F(OCPRecoveryTest, TransportErrorPropagates)
{
    mock.transferError = -EIO;

    EXPECT_EQ(ocp_recov_force_recovery(&ctx), -EIO);

    ocp_recov_device_status status = OCP_RECOV_DEVICE_STATUS_PENDING;
    EXPECT_EQ(ocp_recov_get_device_status(&ctx, &status, nullptr, nullptr),
              -EIO);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(ocp_recov_write_image(&ctx, image.data(), image.size(), nullptr,
                                    nullptr),
              -EIO);
}

TEST_F(OCPRecoveryTest, CustomChunkSizeHonored)
{
    ctx.chunk_size = 32;
    mock.staticResponses[OCP_RECOV_CMD_INDIRECT_STATUS] =
        indirectStatusResponse(0x04);

    std::vector<uint8_t> image(70, 0x11);
    ASSERT_EQ(ocp_recov_write_image(&ctx, image.data(), image.size(), nullptr,
                                    nullptr),
              0);

    std::vector<size_t> chunkSizes;
    for (const auto& w : mock.writes)
    {
        if (w[0] == OCP_RECOV_CMD_INDIRECT_DATA)
        {
            chunkSizes.push_back(w[1]);
        }
    }
    const std::vector<size_t> expectedChunks{32, 32, 6};
    EXPECT_EQ(chunkSizes, expectedChunks);
}

} // namespace
