#include "mock_transport.hpp"

#include <ocp/ocp_recovery.hpp>

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using namespace ocp::recovery;

constexpr uint8_t cmd(Cmd c)
{
    return std::to_underlying(c);
}

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

std::vector<uint8_t> deviceStatusResponse(DeviceStatus status)
{
    return {4, std::to_underlying(status), 0x00, 0x00, 0x00};
}

std::vector<uint8_t> recoveryStatusResponse(RecoveryStatus status)
{
    return {2, std::to_underlying(status), 0x00};
}

std::vector<uint8_t> indirectStatusResponse(uint8_t statusByte)
{
    return {6, statusByte, 0x00, 0x40, 0x00, 0x00, 0x00};
}

const std::error_code ioError = std::make_error_code(std::errc::io_error);

class OCPRecoveryTest : public ::testing::Test
{
  protected:
    MockTransport mock;
    Target target{mock};
};

TEST_F(OCPRecoveryTest, ForceRecoveryEncoding)
{
    ASSERT_TRUE(target.forceRecovery().has_value());

    const std::vector<uint8_t> expected{0x25, 0x03, 0x01, 0x0F, 0x01};
    ASSERT_EQ(mock.writes.size(), 1U);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, RecoveryCtrlEncoding)
{
    ASSERT_TRUE(
        target.recoveryCtrl(1, ImageSelection::fromCms, Activation::activate)
            .has_value());

    const std::vector<uint8_t> expected{0x26, 0x03, 0x01, 0x01, 0x0F};
    ASSERT_EQ(mock.writes.size(), 1U);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, IndirectCtrlEncoding)
{
    ASSERT_TRUE(target.indirectCtrl(0, 0).has_value());
    ASSERT_TRUE(target.indirectCtrl(2, 0x12345678).has_value());

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
    const std::vector<uint8_t> data{0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_TRUE(target.writeIndirectData(data).has_value());

    const std::vector<uint8_t> expected{0x2B, 0x04, 0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_EQ(mock.writes.size(), 1U);
    EXPECT_EQ(mock.writes[0], expected);
}

TEST_F(OCPRecoveryTest, IndirectDataRejectsOversizedChunk)
{
    const std::vector<uint8_t> data(chunkSizeMax + 1, 0xAA);
    auto result = target.writeIndirectData(data);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);

    Target narrow(mock, 32);
    const std::vector<uint8_t> chunk(33, 0xBB);
    result = narrow.writeIndirectData(chunk);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST_F(OCPRecoveryTest, ProtCapDecode)
{
    mock.staticResponses[cmd(Cmd::protCap)] =
        validProtCap(ProtCap::capIndirectCtrl);

    const auto cap = target.getProtCap();
    ASSERT_TRUE(cap.has_value());

    EXPECT_EQ(cap->magic, "OCP RECV");
    EXPECT_EQ(cap->major, 1);
    EXPECT_EQ(cap->minor, 0);
    EXPECT_EQ(cap->caps, ProtCap::capIndirectCtrl);
    EXPECT_EQ(cap->numCms, 2);
    EXPECT_EQ(cap->maxRespTime, 10);
    EXPECT_FALSE(cap->fifoOnly());
}

TEST_F(OCPRecoveryTest, ProtCapBadMagic)
{
    auto resp = validProtCap(0);
    resp[1] = 'X';
    mock.staticResponses[cmd(Cmd::protCap)] = resp;

    const auto cap = target.getProtCap();
    ASSERT_FALSE(cap.has_value());
    EXPECT_EQ(cap.error(), std::errc::protocol_error);
}

TEST_F(OCPRecoveryTest, FifoOnlyDetection)
{
    ProtCap cap{};

    cap.caps = 0;
    EXPECT_FALSE(cap.fifoOnly());

    cap.caps = ProtCap::capIndirectCtrl;
    EXPECT_FALSE(cap.fifoOnly());

    cap.caps = ProtCap::capIndirectCtrl | ProtCap::capIndirectFifo;
    EXPECT_FALSE(cap.fifoOnly());

    cap.caps = ProtCap::capIndirectFifo;
    EXPECT_TRUE(cap.fifoOnly());
}

TEST_F(OCPRecoveryTest, DeviceStatusDecode)
{
    mock.staticResponses[cmd(Cmd::deviceStatus)] = {
        4, std::to_underlying(DeviceStatus::recoveryMode), 0x02, 0x34, 0x12};

    const auto info = target.getDeviceStatus();
    ASSERT_TRUE(info.has_value());

    EXPECT_EQ(info->status, DeviceStatus::recoveryMode);
    EXPECT_EQ(info->protocolError, ProtocolError::unsupportedParameter);
    EXPECT_EQ(info->reason, ReasonCode{0x1234});
}

TEST_F(OCPRecoveryTest, RecoveryStatusDecode)
{
    mock.staticResponses[cmd(Cmd::recoveryStatus)] = {2, 0x13, 0xAB};

    const auto info = target.getRecoveryStatus();
    ASSERT_TRUE(info.has_value());

    EXPECT_EQ(info->status, RecoveryStatus::success);
    EXPECT_EQ(info->imageIndex, 0x01);
    EXPECT_EQ(info->vendorStatus, 0xAB);
}

TEST_F(OCPRecoveryTest, DeviceIdDefaultReadLength)
{
    /* The default read clocks exactly 25 bytes on the wire (count +
     * 24-byte descriptor), matching the fixed PCI/UUID forms. */
    std::vector<uint8_t> resp{24};
    for (uint8_t i = 0; i < 24; i++)
    {
        resp.push_back(i);
    }
    mock.staticResponses[cmd(Cmd::deviceId)] = resp;

    const auto id = target.getDeviceId();
    ASSERT_TRUE(id.has_value());
    ASSERT_EQ(id->size(), 24U);
    EXPECT_EQ(mock.reads.back(), 25U);
}

TEST_F(OCPRecoveryTest, DeviceIdPassesThroughVendorLength)
{
    /* A 40-byte vendor-defined descriptor: longer than the fixed-size
     * PCI/UUID forms, passed through unmodified when the caller asks
     * for a longer read. */
    std::vector<uint8_t> resp{40};
    for (uint8_t i = 0; i < 40; i++)
    {
        resp.push_back(i);
    }
    mock.staticResponses[cmd(Cmd::deviceId)] = resp;

    const auto id = target.getDeviceId(255);
    ASSERT_TRUE(id.has_value());
    ASSERT_EQ(id->size(), 40U);
    EXPECT_EQ((*id)[0], 0);
    EXPECT_EQ((*id)[39], 39);
}

TEST_F(OCPRecoveryTest, OversizedBlockCountRejected)
{
    /* A count larger than the clocked read is a protocol error. */
    mock.staticResponses[cmd(Cmd::deviceStatus)] = {200, 0x01, 0x00, 0x00,
                                                    0x00};

    const auto info = target.getDeviceStatus();
    ASSERT_FALSE(info.has_value());
    EXPECT_EQ(info.error(), std::errc::protocol_error);
}

TEST_F(OCPRecoveryTest, IndirectStatusAckBit)
{
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);

    auto info = target.indirectStatus();
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->ack);
    EXPECT_EQ(info->sizeUnits, 0x40U);
    EXPECT_EQ(info->sizeBytes(), 0x100U);

    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x01);
    info = target.indirectStatus();
    ASSERT_TRUE(info.has_value());
    EXPECT_FALSE(info->ack);
}

TEST_F(OCPRecoveryTest, IndirectDataRead)
{
    mock.staticResponses[cmd(Cmd::indirectData)] = {4, 'L', 'O', 'G', 'S'};

    std::vector<uint8_t> buf(16);
    const auto len = target.readIndirectData(buf);
    ASSERT_TRUE(len.has_value());
    EXPECT_EQ(*len, 4U);
    EXPECT_EQ(std::string(buf.begin(), buf.begin() + 4), "LOGS");
}

TEST_F(OCPRecoveryTest, RecoverHappyPathSequenceAndChunking)
{
    mock.staticResponses[cmd(Cmd::protCap)] =
        validProtCap(ProtCap::capIndirectCtrl);
    mock.staticResponses[cmd(Cmd::deviceStatus)] =
        deviceStatusResponse(DeviceStatus::recoveryMode);
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);
    mock.staticResponses[cmd(Cmd::recoveryStatus)] =
        recoveryStatusResponse(RecoveryStatus::success);

    const std::vector<uint8_t> image(1000, 0x5A);

    size_t lastWritten = 0;
    ASSERT_TRUE(
        target
            .recover(image, [&lastWritten](size_t written,
                                           size_t) { lastWritten = written; })
            .has_value());
    EXPECT_EQ(lastWritten, image.size());

    const std::vector<uint8_t> expectedSequence{
        cmd(Cmd::protCap),      cmd(Cmd::deviceStatus),
        cmd(Cmd::recoveryCtrl), cmd(Cmd::indirectCtrl),
        cmd(Cmd::indirectData), cmd(Cmd::indirectStatus),
        cmd(Cmd::indirectData), cmd(Cmd::indirectStatus),
        cmd(Cmd::indirectData), cmd(Cmd::indirectStatus),
        cmd(Cmd::indirectData), cmd(Cmd::indirectStatus),
        cmd(Cmd::recoveryCtrl), cmd(Cmd::recoveryStatus),
    };
    EXPECT_EQ(mock.commandSequence(), expectedSequence);

    /* 1000 bytes -> 252 + 252 + 252 + 244 */
    std::vector<size_t> chunkSizes;
    for (const auto& w : mock.writes)
    {
        if (w[0] == cmd(Cmd::indirectData))
        {
            chunkSizes.push_back(w[1]);
        }
    }
    const std::vector<size_t> expectedChunks{252, 252, 252, 244};
    EXPECT_EQ(chunkSizes, expectedChunks);

    /* activation is the last RECOVERY_CTRL write */
    const std::vector<uint8_t> activate{
        0x26, 0x03, cmsDefault, std::to_underlying(ImageSelection::fromCms),
        std::to_underlying(Activation::activate)};
    EXPECT_EQ(mock.writes[mock.writes.size() - 2], activate);
}

TEST_F(OCPRecoveryTest, RecoverToleratesMissingProtCap)
{
    mock.commandErrors[cmd(Cmd::protCap)] = ioError;
    mock.staticResponses[cmd(Cmd::deviceStatus)] =
        deviceStatusResponse(DeviceStatus::recoveryMode);
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);
    mock.staticResponses[cmd(Cmd::recoveryStatus)] =
        recoveryStatusResponse(RecoveryStatus::success);

    const std::vector<uint8_t> image(16, 0xA5);
    EXPECT_TRUE(target.recover(image).has_value());
}

TEST_F(OCPRecoveryTest, RecoverRejectsFifoOnlyDevice)
{
    mock.staticResponses[cmd(Cmd::protCap)] =
        validProtCap(ProtCap::capIndirectFifo);

    const std::vector<uint8_t> image(16, 0xA5);
    const auto result = target.recover(image);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::not_supported);
}

TEST_F(OCPRecoveryTest, WriteImageAckTimeout)
{
    /* device never acknowledges the chunk */
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x00);

    const std::vector<uint8_t> image(16, 0xA5);
    const auto result = target.writeImage(image);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::timed_out);
}

TEST_F(OCPRecoveryTest, TransportErrorPropagates)
{
    mock.transferError = ioError;

    const auto forced = target.forceRecovery();
    ASSERT_FALSE(forced.has_value());
    EXPECT_EQ(forced.error(), ioError);

    const auto status = target.getDeviceStatus();
    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), ioError);

    const std::vector<uint8_t> image(16, 0xA5);
    const auto written = target.writeImage(image);
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error(), ioError);
}

TEST_F(OCPRecoveryTest, CustomChunkSizeHonored)
{
    Target narrow(mock, 32);
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);

    const std::vector<uint8_t> image(70, 0x11);
    ASSERT_TRUE(narrow.writeImage(image).has_value());

    std::vector<size_t> chunkSizes;
    for (const auto& w : mock.writes)
    {
        if (w[0] == cmd(Cmd::indirectData))
        {
            chunkSizes.push_back(w[1]);
        }
    }
    const std::vector<size_t> expectedChunks{32, 32, 6};
    EXPECT_EQ(chunkSizes, expectedChunks);
}

TEST_F(OCPRecoveryTest, ShortIndirectStatusStillAcks)
{
    /* Status-only acknowledge response (count = 1, no size field):
     * tolerated, size reported as 0. */
    mock.staticResponses[cmd(Cmd::indirectStatus)] = {1, 0x04};

    const auto info = target.indirectStatus();
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->ack);
    EXPECT_EQ(info->sizeUnits, 0U);
}

TEST_F(OCPRecoveryTest, WriteImageRetriesThenSucceeds)
{
    /* First chunk write is not acknowledged within the poll budget;
     * the retry write is. Scripted via the response queue: one no-ack
     * response per poll of the first attempt, then acks. */
    for (unsigned int poll = 0; poll < ackPollRetries; poll++)
    {
        mock.queuedResponses[cmd(Cmd::indirectStatus)].push_back(
            indirectStatusResponse(0x00));
    }
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);

    const std::vector<uint8_t> image(16, 0xA5);
    ASSERT_TRUE(target.writeImage(image).has_value());

    /* two INDIRECT_DATA writes: the unacknowledged attempt + the retry */
    size_t dataWrites = 0;
    for (const auto& w : mock.writes)
    {
        if (w[0] == cmd(Cmd::indirectData))
        {
            dataWrites++;
        }
    }
    EXPECT_EQ(dataWrites, 2U);
}

TEST_F(OCPRecoveryTest, RecoverForcesRecoveryMode)
{
    /* Device starts in a fatal state; recover() must issue DEVICE_RESET
     * and poll until the device reports recovery mode. */
    mock.queuedResponses[cmd(Cmd::deviceStatus)].push_back(
        deviceStatusResponse(DeviceStatus::fatalError));
    mock.staticResponses[cmd(Cmd::deviceStatus)] =
        deviceStatusResponse(DeviceStatus::recoveryMode);
    mock.staticResponses[cmd(Cmd::protCap)] =
        validProtCap(ProtCap::capIndirectCtrl);
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);
    mock.staticResponses[cmd(Cmd::recoveryStatus)] =
        recoveryStatusResponse(RecoveryStatus::success);

    const std::vector<uint8_t> image(16, 0xA5);
    ASSERT_TRUE(target.recover(image).has_value());

    const std::vector<uint8_t> forceReset{0x25, 0x03, 0x01, 0x0F, 0x01};
    bool sawReset = false;
    for (const auto& w : mock.writes)
    {
        if (w == forceReset)
        {
            sawReset = true;
        }
    }
    EXPECT_TRUE(sawReset);
}

TEST_F(OCPRecoveryTest, RecoverFailsOnRecoveryError)
{
    mock.staticResponses[cmd(Cmd::deviceStatus)] =
        deviceStatusResponse(DeviceStatus::recoveryMode);
    mock.staticResponses[cmd(Cmd::indirectStatus)] =
        indirectStatusResponse(0x04);
    mock.staticResponses[cmd(Cmd::recoveryStatus)] =
        recoveryStatusResponse(RecoveryStatus::authError);

    const std::vector<uint8_t> image(16, 0xA5);
    const auto result = target.recover(image);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::io_error);
}

TEST_F(OCPRecoveryTest, DrainedIndirectReadReturnsZero)
{
    /* A zero block count on INDIRECT_DATA reads means the window is
     * drained, not a protocol error. */
    mock.staticResponses[cmd(Cmd::indirectData)] = {0};

    std::vector<uint8_t> buf(16);
    const auto len = target.readIndirectData(buf);
    ASSERT_TRUE(len.has_value());
    EXPECT_EQ(*len, 0U);
}

} // namespace
