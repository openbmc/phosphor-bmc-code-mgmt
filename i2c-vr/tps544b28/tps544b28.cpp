#include "tps544b28.hpp"

#include "common/include/host_power.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

using namespace phosphor::software::host_power;

namespace
{

// PMBus command codes.
constexpr uint8_t regStoreUserAll = 0x15;   // Send Byte
constexpr uint8_t regRestoreUserAll = 0x16; // Send Byte
constexpr uint8_t regPasskey = 0xD2;        // Block Write / Block Read
constexpr uint8_t regIcDeviceId = 0xAD;     // Block Read, 6 data bytes
constexpr uint8_t regNvmChecksum = 0xD9;    // Read Word

// IC_DEVICE_ID returns six data bytes. Only the first five identify the
// part; the sixth is not compared.
constexpr std::array<uint8_t, 5> deviceIdB28{0x54, 0x49, 0x54, 0x4B, 0x28};
constexpr size_t deviceIdDataLen = 6;

// Hold time after STORE_USER_ALL, modelled on TPS544x27 getStoreDelay():
// take the Pause that immediately follows STORE_USER_ALL in the script, use
// the minimum when there is none, and reject a script that asks for less,
// since a hold time that short suggests a corrupt or wrong file.
constexpr std::chrono::milliseconds minStoreDelay{200};

// Read-only commands. Writes to these are skipped with a warning instead of
// failing the update, as TPS544x27 does.
//
// MFR_ID (99h) is deliberately not listed: TI's scripts write it, and it can
// be written and read back.
bool isReadOnlyPerDatasheet(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x19: // CAPABILITY
        case 0x20: // VOUT_MODE
        case 0x29: // VOUT_SCALE_LOOP
        case 0x65: // TOFF_FALL
        case 0x78: // STATUS_BYTE
        case 0x79: // STATUS_WORD
        case 0x7E: // STATUS_CML
        case 0x80: // STATUS_MFR_SPECIFIC
        case 0x8B: // READ_VOUT
        case 0x8C: // READ_IOUT
        case 0x8D: // READ_TEMP1
        case 0x98: // PMBUS_REVISION
        case 0x9A: // MFR_MODEL
        case 0xAD: // IC_DEVICE_ID
        case 0xAE: // IC_DEVICE_REV
        case 0xD9: // NVM_CHECKSUM
        case 0xFC: // FUSION_ID0
        case 0xFD: // FUSION_ID1
            return true;
        default:
            return false;
    }
}

// Writing PASSKEY enables password protection and can lock the device out
// permanently, so a script that tries to is rejected outright.
bool isForbiddenWrite(uint8_t cmd)
{
    return cmd == regPasskey;
}

} // namespace

sdbusplus::async::task<bool> TPS544B28::sendByte(uint8_t cmd)
{
    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf;

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544B28 SendByte failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::writeBytes(
    uint8_t cmd, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> tbuf{cmd};
    tbuf.insert(tbuf.end(), data.begin(), data.end());
    std::vector<uint8_t> rbuf;

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544B28 write failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::readBytes(
    uint8_t cmd, size_t readLen, std::vector<uint8_t>& data)
{
    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf(readLen);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544B28 read failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    data = std::move(rbuf);
    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::readWord(uint8_t cmd, uint16_t* value)
{
    if (value == nullptr)
    {
        error("TPS544B28 readWord invalid input");
        co_return false;
    }

    std::vector<uint8_t> raw;
    if (!co_await readBytes(cmd, 2, raw))
    {
        co_return false;
    }

    // SMBus Read Word returns the low byte first.
    *value = static_cast<uint16_t>(raw[0]) |
             static_cast<uint16_t>(static_cast<uint16_t>(raw[1]) << 8);

    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::checkIcDeviceId()
{
    // Block Read returns [byte count][data...].
    std::vector<uint8_t> raw;

    if (!co_await readBytes(regIcDeviceId, deviceIdDataLen + 1, raw))
    {
        error("TPS544B28 failed to read IC_DEVICE_ID");
        co_return false;
    }

    if (raw.size() != deviceIdDataLen + 1 || raw[0] != deviceIdDataLen)
    {
        error("TPS544B28 invalid IC_DEVICE_ID response, bytes={BYTES}",
              "BYTES", bytesToHex(raw));
        co_return false;
    }

    if (!std::equal(deviceIdB28.begin(), deviceIdB28.end(), raw.begin() + 1))
    {
        error("TPS544B28 IC_DEVICE_ID mismatch, refusing to program, "
              "id={ID}",
              "ID", bytesToHex(raw));
        co_return false;
    }

    debug("TPS544B28 IC_DEVICE_ID verified, id={ID}", "ID", bytesToHex(raw));
    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::executeCommand(const CsvCommand& cmd)
{
    switch (cmd.op)
    {
        case CsvOp::WriteByte:
        case CsvOp::WriteWord:
        case CsvOp::BlockWrite:
        {
            // PASSKEY writes are rejected for the whole image in
            // verifyImage(), before any bus traffic, so they never get here.
            if (isReadOnlyPerDatasheet(cmd.cmd))
            {
                warning("TPS544B28 skipping write to a read only command, "
                        "line={LINE}, cmd={CMD}",
                        "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return true;
            }

            co_return co_await writeBytes(cmd.cmd, cmd.dataBytes);
        }

        case CsvOp::ReadByte:
        case CsvOp::ReadWord:
        case CsvOp::BlockRead:
        {
            // The script carries the expected response for every read, which
            // is how it verifies IC_DEVICE_ID and every register it just
            // wrote. Treat a mismatch as a failure.
            std::vector<uint8_t> actual;
            if (!co_await readBytes(cmd.cmd, cmd.expectedBytes.size(), actual))
            {
                co_return false;
            }

            if (actual != cmd.expectedBytes)
            {
                error("TPS544B28 read back mismatch, line={LINE}, cmd={CMD}, "
                      "expected={EXPECTED}, actual={ACTUAL}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd,
                      "EXPECTED", bytesToHex(cmd.expectedBytes), "ACTUAL",
                      bytesToHex(actual));
                co_return false;
            }

            debug("TPS544B28 read back matched, line={LINE}, cmd={CMD}",
                  "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
            co_return true;
        }

        case CsvOp::SendByte:
            if (cmd.cmd == regStoreUserAll || cmd.cmd == regRestoreUserAll)
            {
                // Handled by the caller so the hold time is always applied.
                co_return true;
            }
            co_return co_await sendByte(cmd.cmd);

        case CsvOp::Pause:
            co_await sdbusplus::async::sleep_for(
                ctx, std::chrono::milliseconds(cmd.pauseMs));
            co_return true;

        case CsvOp::Reset:
            // verifyImage() rejects a Reset before STORE_USER_ALL and
            // executeUntilStore() stops at STORE_USER_ALL, so this is
            // unreachable.
            co_return true;
    }

    error("TPS544B28 unhandled script operation, line={LINE}", "LINE",
          cmd.lineNo);
    co_return false;
}

bool TPS544B28::getStoreDelay(std::chrono::milliseconds& delay) const
{
    const auto& commands = csvParser.getParsedImage().commands;

    for (size_t i = 0; i < commands.size(); i++)
    {
        if (commands[i].op != CsvOp::SendByte ||
            commands[i].cmd != regStoreUserAll)
        {
            continue;
        }

        if (i + 1 < commands.size() && commands[i + 1].op == CsvOp::Pause)
        {
            delay = std::chrono::milliseconds(commands[i + 1].pauseMs);

            if (delay < minStoreDelay)
            {
                error("TPS544B28 script hold time after STORE_USER_ALL is "
                      "below the minimum, line={LINE}, delayMs={DELAY_MS}, "
                      "minimumMs={MINIMUM_MS}",
                      "LINE", commands[i + 1].lineNo, "DELAY_MS",
                      delay.count(), "MINIMUM_MS", minStoreDelay.count());
                return false;
            }

            debug("TPS544B28 using script hold time after STORE_USER_ALL, "
                  "line={LINE}, delayMs={DELAY_MS}",
                  "LINE", commands[i + 1].lineNo, "DELAY_MS", delay.count());
        }
        else
        {
            delay = minStoreDelay;
            debug("TPS544B28 no Pause after STORE_USER_ALL, using default "
                  "delayMs={DELAY_MS}",
                  "DELAY_MS", delay.count());
        }

        return true;
    }

    error("TPS544B28 script does not contain STORE_USER_ALL (SendByte 0x15)");
    return false;
}

sdbusplus::async::task<bool> TPS544B28::executeUntilStore()
{
    for (const auto& cmd : csvParser.getParsedImage().commands)
    {
        if (cmd.op == CsvOp::SendByte && cmd.cmd == regStoreUserAll)
        {
            debug("TPS544B28 reached STORE_USER_ALL at line {LINE}", "LINE",
                  cmd.lineNo);
            co_return true;
        }

        if (!co_await executeCommand(cmd))
        {
            error("TPS544B28 script failed at line {LINE}", "LINE",
                  cmd.lineNo);
            co_return false;
        }
    }

    // verifyImage() already guarantees STORE_USER_ALL is present, so the loop
    // always returns above. Kept so a future change that skips verification
    // fails closed instead of reporting success.
    error("TPS544B28 script ended without STORE_USER_ALL");
    co_return false;
}

sdbusplus::async::task<bool> TPS544B28::storeUserAll()
{
    debug("TPS544B28 issuing STORE_USER_ALL, delayMs={DELAY_MS}", "DELAY_MS",
          storeDelay.count());

    if (!co_await sendByte(regStoreUserAll))
    {
        error("TPS544B28 STORE_USER_ALL failed");
        co_return false;
    }

    // The device NACKs everything but its address while the EEPROM is being
    // programmed, so nothing may be sent during this window.
    co_await sdbusplus::async::sleep_for(ctx, storeDelay);

    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::getCRC(uint32_t* sum)
{
    if (sum == nullptr)
    {
        error("TPS544B28 getCRC invalid input");
        co_return false;
    }

    // NVM_CHECKSUM is a CRC-16 of the stored NVM that the device computes
    // itself, read as a word.
    uint16_t checksum = 0;

    if (!co_await readWord(regNvmChecksum, &checksum))
    {
        error("TPS544B28 failed to read NVM_CHECKSUM");
        co_return false;
    }

    debug("TPS544B28 NVM_CHECKSUM={CHECKSUM}", "CHECKSUM", lg2::hex, checksum);

    *sum = static_cast<uint32_t>(checksum);
    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::verifyImage(const uint8_t* image,
                                                    size_t imageSize)
{
    // Everything that can be decided from the image alone is decided here,
    // before updateFirmware() touches the bus. The framework calls this first
    // (I2CVRDevice::updateDevice), so a bad image never reaches the device.
    std::string err;

    if (!csvParser.parseImage(image, imageSize, err))
    {
        error("TPS544B28 image parse failed: {ERR}", "ERR", err);
        co_return false;
    }

    const auto& parsed = csvParser.getParsedImage();

    // Export options this driver has been checked against.
    if (parsed.pecEnabled)
    {
        error("TPS544B28 script was exported with PEC=True, which this "
              "driver has never been tested against");
        co_return false;
    }

    if (parsed.hasAddress)
    {
        error("TPS544B28 script was exported with HasAddress=True, which "
              "this driver does not support");
        co_return false;
    }

    if (!parsed.wordLowByteFirst)
    {
        error("TPS544B28 script was exported with word bytes high first, "
              "which this driver does not support");
        co_return false;
    }

    // Block data is sent and compared as written, length byte included, so a
    // script without the length byte would be framed wrongly on the bus.
    // TPS544x27 executeParsedImage() rejects this case too.
    if (!parsed.includeBlockLength)
    {
        error("TPS544B28 script was exported with IncludeBlockLength=False, "
              "which this driver does not support");
        co_return false;
    }

    // The script must identify the part before writing to it.
    const bool checksIdentity = std::any_of(
        parsed.commands.begin(), parsed.commands.end(),
        [](const CsvCommand& cmd) {
            return cmd.op == CsvOp::BlockRead && cmd.cmd == regIcDeviceId;
        });

    if (!checksIdentity)
    {
        error("TPS544B28 script does not verify IC_DEVICE_ID, refusing to "
              "program");
        co_return false;
    }

    // Walk the part of the script that will actually run.
    for (const auto& cmd : parsed.commands)
    {
        if (cmd.op == CsvOp::SendByte && cmd.cmd == regStoreUserAll)
        {
            break;
        }

        const bool isWrite = cmd.op == CsvOp::WriteByte ||
                             cmd.op == CsvOp::WriteWord ||
                             cmd.op == CsvOp::BlockWrite;

        if (isWrite && isForbiddenWrite(cmd.cmd))
        {
            error("TPS544B28 script writes PASSKEY, refusing to program, "
                  "line={LINE}",
                  "LINE", cmd.lineNo);
            co_return false;
        }

        if (cmd.op == CsvOp::Reset)
        {
            // The BMC cannot power cycle the part part-way through a script.
            error("TPS544B28 script asks for a reset before STORE_USER_ALL, "
                  "line={LINE}",
                  "LINE", cmd.lineNo);
            co_return false;
        }
    }

    // Also confirms STORE_USER_ALL is present.
    if (!getStoreDelay(storeDelay))
    {
        co_return false;
    }

    info("TPS544B28 script verified, commands={COUNT}, storeDelayMs={DELAY_MS}",
         "COUNT", parsed.commands.size(), "DELAY_MS", storeDelay.count());

    co_return true;
}

sdbusplus::async::task<bool> TPS544B28::updateFirmware(bool force)
{
    (void)force;

    // Programming flow:
    //   BlockRead ADh -> match -> write registers -> SendByte 15h ->
    //   pause -> power cycle -> ReadWord D9h.
    //
    // Only the steps up to the pause happen here. The power cycle is driven
    // by the framework, which allows the OnReset apply time only, and the
    // checksum is read back by getCRC() the next time the service starts.
    //
    // Same guard as TPS544x27 executeParsedImage(): do not reconfigure the
    // regulator while the host is running.
    const auto powerState = co_await HostPower::getState(ctx);

    if (powerState != stateOff)
    {
        error("TPS544B28 update rejected: host power must be off before VR "
              "update");
        co_return false;
    }

    if (!co_await checkIcDeviceId())
    {
        co_return false;
    }

    if (!co_await executeUntilStore())
    {
        co_return false;
    }

    if (!co_await storeUserAll())
    {
        co_return false;
    }

    info("TPS544B28 configuration stored, a power cycle is required before "
         "the new NVM_CHECKSUM takes effect");

    co_return true;
}

bool TPS544B28::forcedUpdateAllowed()
{
    return true;
}

} // namespace phosphor::software::VR
