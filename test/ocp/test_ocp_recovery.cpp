#include <ocp/ocp_recovery.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <vector>

#include <gtest/gtest.h>

namespace
{

/* Register-level scripted transport: records every write message and
 * answers reads per command code, either from a queue (consumed in order)
 * or from a static response. */
struct MockTransport
{
    std::vector<std::vector<uint8_t>> writes;
    std::map<uint8_t, std::vector<uint8_t>> staticResponses;
    std::map<uint8_t, std::deque<std::vector<uint8_t>>> queuedResponses;
    std::map<uint8_t, int> commandErrors;
    int transferError = 0;

    std::vector<uint8_t> commandSequence() const
    {
        std::vector<uint8_t> seq;
        seq.reserve(writes.size());
        for (const auto& w : writes)
        {
            seq.push_back(w[0]);
        }
        return seq;
    }
};

int mockTransfer(void* user, const uint8_t* wbuf, size_t wlen, uint8_t* rbuf,
                 size_t rlen)
{
    auto* mock = static_cast<MockTransport*>(user);
    const uint8_t cmd = wbuf[0];

    if (mock->transferError != 0)
    {
        return mock->transferError;
    }

    auto errIt = mock->commandErrors.find(cmd);
    if (errIt != mock->commandErrors.end())
    {
        return errIt->second;
    }

    mock->writes.emplace_back(wbuf, wbuf + wlen);

    if (rlen == 0)
    {
        return 0;
    }

    std::memset(rbuf, 0, rlen);

    auto queueIt = mock->queuedResponses.find(cmd);
    if (queueIt != mock->queuedResponses.end() && !queueIt->second.empty())
    {
        const auto& resp = queueIt->second.front();
        std::memcpy(rbuf, resp.data(), std::min(rlen, resp.size()));
        queueIt->second.pop_front();
        return 0;
    }

    auto respIt = mock->staticResponses.find(cmd);
    if (respIt != mock->staticResponses.end())
    {
        const auto& resp = respIt->second;
        std::memcpy(rbuf, resp.data(), std::min(rlen, resp.size()));
        return 0;
    }

    return 0;
}

std::vector<uint8_t> validProtCap(uint16_t caps)
{
    return {15,  'O', 'C', 'P',
            ' ', 'R', 'E', 'C',
            'V', 1,   0,   static_cast<uint8_t>(caps & 0xFF),
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
        ASSERT_EQ(ocp_init_transport(&ctx, mockTransfer, &mock, 0x6F), 0);
    }

    MockTransport mock;
    struct ocp_ctx ctx = {};
};

TEST_F(OCPRecoveryTest, ForceRecoveryEncoding)
{
    ASSERT_EQ(ocp_force_recovery(&ctx), 0);

    const std::vector<uint8_t> expected{0x25, 0x03, 0x01, 0x0F, 0x01};
    ASSERT_EQ(mock.writes.size(), 1u);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, RecoveryCtrlEncoding)
{
    ASSERT_EQ(ocp_recovery_ctrl(&ctx, 1, OCP_RECOVERY_IMAGE_FROM_CMS,
                                OCP_RECOVERY_ACTIVATE),
              0);

    const std::vector<uint8_t> expected{0x26, 0x03, 0x01, 0x01, 0x0F};
    ASSERT_EQ(mock.writes.size(), 1u);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, IndirectCtrlEncoding)
{
    ASSERT_EQ(ocp_indirect_ctrl(&ctx, 0, 0), 0);
    ASSERT_EQ(ocp_indirect_ctrl(&ctx, 2, 0x12345678), 0);

    const std::vector<uint8_t> zeroOffset{0x29, 0x06, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> leOffset{0x29, 0x06, 0x02, 0x00,
                                        0x78, 0x56, 0x34, 0x12};
    ASSERT_EQ(mock.writes.size(), 2u);
    EXPECT_EQ(mock.writes[0], zeroOffset);
    EXPECT_EQ(mock.writes[1], leOffset);
}

TEST_F(OCPRecoveryTest, IndirectDataFraming)
{
    const uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(ocp_indirect_data_write(&ctx, data, sizeof(data)), 0);

    const std::vector<uint8_t> expected{0x2B, 0x04, 0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(mock.writes.size(), 1u);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, IndirectDataRejectsOversizedChunk)
{
    std::vector<uint8_t> data(OCP_CHUNK_SIZE_MAX + 1, 0xAA);
    EXPECT_EQ(ocp_indirect_data_write(&ctx, data.data(), data.size()),
              -EINVAL);

    ctx.chunk_size = 32;
    std::vector<uint8_t> chunk(33, 0xBB);
    EXPECT_EQ(ocp_indirect_data_write(&ctx, chunk.data(), chunk.size()),
              -EINVAL);
}

TEST_F(OCPRecoveryTest, ProtCapDecode)
{
    mock.staticResponses[OCP_CMD_PROT_CAP] =
        validProtCap(OCP_PROT_CAP_INDIRECT_CTRL);

    struct ocp_prot_cap cap = {};
    ASSERT_EQ(ocp_get_prot_cap(&ctx, &cap), 0);

    EXPECT_STREQ(cap.magic, "OCP RECV");
    EXPECT_EQ(cap.major, 1);
    EXPECT_EQ(cap.minor, 0);
    EXPECT_EQ(cap.caps, OCP_PROT_CAP_INDIRECT_CTRL);
    EXPECT_EQ(cap.num_cms, 2);
    EXPECT_EQ(cap.max_resp_time, 10);
}

TEST_F(OCPRecoveryTest, ProtCapBadMagic)
{
    auto resp = validProtCap(0);
    resp[1] = 'X';
    mock.staticResponses[OCP_CMD_PROT_CAP] = resp;

    struct ocp_prot_cap cap = {};
    EXPECT_EQ(ocp_get_prot_cap(&ctx, &cap), -EPROTO);
}

TEST_F(OCPRecoveryTest, CheckCaps)
{
    struct ocp_prot_cap cap = {};

    cap.caps = 0;
    EXPECT_EQ(ocp_check_caps(&cap), 0);

    cap.caps = OCP_PROT_CAP_INDIRECT_CTRL;
    EXPECT_EQ(ocp_check_caps(&cap), 0);

    cap.caps = OCP_PROT_CAP_INDIRECT_CTRL | OCP_PROT_CAP_INDIRECT_FIFO;
    EXPECT_EQ(ocp_check_caps(&cap), 0);

    cap.caps = OCP_PROT_CAP_INDIRECT_FIFO;
    EXPECT_EQ(ocp_check_caps(&cap), -ENOTSUP);
}

TEST_F(OCPRecoveryTest, DeviceStatusDecode)
{
    mock.staticResponses[OCP_CMD_DEVICE_STATUS] = {
        4, OCP_DEVICE_STATUS_RECOVERY_MODE, 0x02, 0x34, 0x12};

    uint8_t status = 0;
    uint8_t protErr = 0;
    uint16_t reason = 0;
    ASSERT_EQ(ocp_get_device_status(&ctx, &status, &protErr, &reason), 0);

    EXPECT_EQ(status, OCP_DEVICE_STATUS_RECOVERY_MODE);
    EXPECT_EQ(protErr, 0x02);
    EXPECT_EQ(reason, 0x1234);
}

TEST_F(OCPRecoveryTest, RecoveryStatusDecode)
{
    mock.staticResponses[OCP_CMD_RECOVERY_STATUS] = {2, 0x13, 0xAB};

    uint8_t status = 0;
    uint8_t index = 0;
    uint8_t vendor = 0;
    ASSERT_EQ(ocp_get_recovery_status(&ctx, &status, &index, &vendor), 0);

    EXPECT_EQ(status, OCP_RECOVERY_STATUS_SUCCESS);
    EXPECT_EQ(index, 0x01);
    EXPECT_EQ(vendor, 0xAB);
}

TEST_F(OCPRecoveryTest, IndirectStatusAckBit)
{
    mock.staticResponses[OCP_CMD_INDIRECT_STATUS] = indirectStatusResponse(
        0x04);

    uint8_t status = 0;
    bool ack = false;
    uint32_t size = 0;
    ASSERT_EQ(ocp_indirect_status(&ctx, &status, &ack, &size), 0);
    EXPECT_TRUE(ack);
    EXPECT_EQ(size, 0x40u);

    mock.staticResponses[OCP_CMD_INDIRECT_STATUS] = indirectStatusResponse(
        0x01);
    ASSERT_EQ(ocp_indirect_status(&ctx, &status, &ack, nullptr), 0);
    EXPECT_FALSE(ack);
}

TEST_F(OCPRecoveryTest, IndirectDataRead)
{
    mock.staticResponses[OCP_CMD_INDIRECT_DATA] = {4, 'L', 'O', 'G', 'S'};

    uint8_t buf[16] = {};
    size_t out = 0;
    ASSERT_EQ(ocp_indirect_data_read(&ctx, buf, sizeof(buf), &out), 0);
    EXPECT_EQ(out, 4u);
    EXPECT_EQ(std::memcmp(buf, "LOGS", 4), 0);
}

TEST_F(OCPRecoveryTest, RecoverHappyPathSequenceAndChunking)
{
    mock.staticResponses[OCP_CMD_PROT_CAP] =
        validProtCap(OCP_PROT_CAP_INDIRECT_CTRL);
    mock.staticResponses[OCP_CMD_DEVICE_STATUS] =
        deviceStatusResponse(OCP_DEVICE_STATUS_RECOVERY_MODE);
    mock.staticResponses[OCP_CMD_INDIRECT_STATUS] = indirectStatusResponse(
        0x04);
    mock.staticResponses[OCP_CMD_RECOVERY_STATUS] =
        recoveryStatusResponse(OCP_RECOVERY_STATUS_SUCCESS);

    std::vector<uint8_t> image(1000, 0x5A);

    size_t lastWritten = 0;
    auto progress = [](void* user, size_t written, size_t) {
        *static_cast<size_t*>(user) = written;
    };

    ASSERT_EQ(ocp_recover(&ctx, image.data(), image.size(), progress,
                          &lastWritten),
              0);
    EXPECT_EQ(lastWritten, image.size());

    const std::vector<uint8_t> expectedSequence{
        OCP_CMD_PROT_CAP,        OCP_CMD_DEVICE_STATUS,
        OCP_CMD_RECOVERY_CTRL,   OCP_CMD_INDIRECT_CTRL,
        OCP_CMD_INDIRECT_DATA,   OCP_CMD_INDIRECT_STATUS,
        OCP_CMD_INDIRECT_DATA,   OCP_CMD_INDIRECT_STATUS,
        OCP_CMD_INDIRECT_DATA,   OCP_CMD_INDIRECT_STATUS,
        OCP_CMD_INDIRECT_DATA,   OCP_CMD_INDIRECT_STATUS,
        OCP_CMD_RECOVERY_CTRL,   OCP_CMD_RECOVERY_STATUS,
    };
    EXPECT_EQ(mock.commandSequence(), expectedSequence);

    /* 1000 bytes -> 252 + 252 + 252 + 244 */
    std::vector<size_t> chunkSizes;
    for (const auto& w : mock.writes)
    {
        if (w[0] == OCP_CMD_INDIRECT_DATA)
        {
            chunkSizes.push_back(w[1]);
        }
    }
    const std::vector<size_t> expectedChunks{252, 252, 252, 244};
    EXPECT_EQ(chunkSizes, expectedChunks);

    /* activation is the last RECOVERY_CTRL write */
    const std::vector<uint8_t> activate{0x26, 0x03, OCP_CMS_DEFAULT,
                                        OCP_RECOVERY_IMAGE_FROM_CMS,
                                        OCP_RECOVERY_ACTIVATE};
    EXPECT_EQ(mock.writes[mock.writes.size() - 2], activate);
}

TEST_F(OCPRecoveryTest, RecoverToleratesMissingProtCap)
{
    mock.commandErrors[OCP_CMD_PROT_CAP] = -EIO;
    mock.staticResponses[OCP_CMD_DEVICE_STATUS] =
        deviceStatusResponse(OCP_DEVICE_STATUS_RECOVERY_MODE);
    mock.staticResponses[OCP_CMD_INDIRECT_STATUS] = indirectStatusResponse(
        0x04);
    mock.staticResponses[OCP_CMD_RECOVERY_STATUS] =
        recoveryStatusResponse(OCP_RECOVERY_STATUS_SUCCESS);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(ocp_recover(&ctx, image.data(), image.size(), nullptr, nullptr),
              0);
}

TEST_F(OCPRecoveryTest, RecoverRejectsFifoOnlyDevice)
{
    mock.staticResponses[OCP_CMD_PROT_CAP] =
        validProtCap(OCP_PROT_CAP_INDIRECT_FIFO);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(ocp_recover(&ctx, image.data(), image.size(), nullptr, nullptr),
              -ENOTSUP);
}

TEST_F(OCPRecoveryTest, WriteImageAckTimeout)
{
    /* device never acknowledges the chunk */
    mock.staticResponses[OCP_CMD_INDIRECT_STATUS] = indirectStatusResponse(
        0x00);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(ocp_write_image(&ctx, image.data(), image.size(), nullptr,
                              nullptr),
              -ETIMEDOUT);
}

TEST_F(OCPRecoveryTest, TransportErrorPropagates)
{
    mock.transferError = -EIO;

    EXPECT_EQ(ocp_force_recovery(&ctx), -EIO);

    uint8_t status = 0;
    EXPECT_EQ(ocp_get_device_status(&ctx, &status, nullptr, nullptr), -EIO);

    std::vector<uint8_t> image(16, 0xA5);
    EXPECT_EQ(ocp_write_image(&ctx, image.data(), image.size(), nullptr,
                              nullptr),
              -EIO);
}

TEST_F(OCPRecoveryTest, CustomChunkSizeHonored)
{
    ctx.chunk_size = 32;
    mock.staticResponses[OCP_CMD_INDIRECT_STATUS] = indirectStatusResponse(
        0x04);

    std::vector<uint8_t> image(70, 0x11);
    ASSERT_EQ(ocp_write_image(&ctx, image.data(), image.size(), nullptr,
                              nullptr),
              0);

    std::vector<size_t> chunkSizes;
    for (const auto& w : mock.writes)
    {
        if (w[0] == OCP_CMD_INDIRECT_DATA)
        {
            chunkSizes.push_back(w[1]);
        }
    }
    const std::vector<size_t> expectedChunks{32, 32, 6};
    EXPECT_EQ(chunkSizes, expectedChunks);
}

} // namespace
