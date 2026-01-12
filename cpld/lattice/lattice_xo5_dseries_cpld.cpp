#include "lattice_xo5_dseries_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

namespace
{
/* ESFB packet header */
static constexpr uint8_t esfbHeader = 0xBC;

/* ESFB command length limits */
static constexpr size_t esfbDataMaxSize = 245;
static constexpr size_t esfbPackageLen = 128;

/* ESFB Packet fragment flags */
static constexpr uint16_t esfbFirstPacket = 0x8000;
static constexpr uint16_t esfbLastPacket = 0x4000;
static constexpr uint16_t esfbFirstAndLastPacket = 0xC000;
static constexpr size_t esfbFirstPacketNum = 1;

/* ESFB Misc settings */
static constexpr auto esfbSleepInterval = std::chrono::milliseconds(5);
static constexpr size_t esfbRetryCount = 5000;
static constexpr size_t packetImageIdSize = 1;
static constexpr uint8_t dryRunResultSuccess = 1;

/* ESFB packet layout positions */
static constexpr size_t esfbHeaderPos = 0;
static constexpr size_t esfbLengthPos = 4;
static constexpr size_t esfbReceiveDataPos = 6;
static constexpr size_t esfbReceiveStatusPos = 6;
static constexpr size_t esfbReceiveLenPos = 7;

/* ESFB fixed packet lengths */
static constexpr size_t esfbReadStatusLength = 10;
static constexpr size_t esfbReadStableLength = 7;
static constexpr size_t esfbWriteCmdLength = 6;
} // namespace

uint8_t LatticeXO5DSeriesCPLD::checkSum(const std::vector<uint8_t>& vec)
{
    uint8_t checksum = 0;

    for (size_t i = 0; (i + 1) < vec.size(); ++i)
    {
        checksum += vec[i];
    }

    return checksum;
}

/*
 * Request packet format:
 * |1 byte|1 byte|   2 bytes   |1 byte|max 245 bytes| 1 byte |
 * |Header|Cmd id|Fragment flag|Lenght|     Data    |Checksum|
 */
sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::esfbWrite(
    uint16_t fragmentFlag, uint8_t cmdId, std::vector<uint8_t>& data)
{
    if (data.size() > esfbDataMaxSize)
    {
        lg2::error("ESFB write payload exceeds max size: {SIZE}", "SIZE",
                   data.size());
        co_return false;
    }

    size_t packetLength = esfbWriteCmdLength + data.size();
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;

    request.reserve(packetLength);
    request.push_back(esfbHeader);
    request.push_back(cmdId);
    request.push_back(static_cast<uint8_t>(fragmentFlag));
    request.push_back(static_cast<uint8_t>(fragmentFlag >> 8));
    request.push_back(static_cast<uint8_t>(packetLength));
    request.insert(request.end(), data.begin(), data.end());
    request.push_back(checkSum(request));

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB write failed for CMD: {CMD_ID}", "CMD_ID", lg2::hex,
                   cmdId);
        co_return false;
    }

    co_return true;
}

/*
 * Response packet format:
 * |1 byte|1 byte|   2 bytes   |1 byte|1 byte|1 byte|2 bytes| 1 byte |
 * |Header|Cmd id|Fragment flag|Lenght| Type |status|  Data |Checksum|
 */
sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::esfbReadStatus(
    uint8_t cmdId, LatticeXO5DSeriesCPLD::EsfbStatus& status, uint16_t& data)
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> response(esfbReadStatusLength, 0);

    request.push_back(cmdId);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB read status failed for CMD: {CMD}", "CMD", lg2::hex,
                   cmdId);
        co_return false;
    }

    if (response.size() < esfbReadStatusLength)
    {
        lg2::error("ESFB read status: response length too short. Len: {LEN}",
                   "LEN", response.size());
        co_return false;
    }

    if (response[esfbHeaderPos] != esfbHeader)
    {
        lg2::error("ESFB read status: response header is invalid.");
        co_return false;
    }

    if (response[esfbLengthPos] != esfbReadStatusLength)
    {
        lg2::error("ESFB read status: response length is invalid.");
        co_return false;
    }

    if (response.back() != checkSum(response))
    {
        lg2::error("ESFB read status: response checksum is invalid.");
        co_return false;
    }

    status = static_cast<EsfbStatus>(response[esfbReceiveStatusPos]);
    if (status == EsfbStatus::busy || status == EsfbStatus::dryRun)
    {
        co_return false;
    }

    data = static_cast<uint16_t>(
        (static_cast<uint16_t>(response[esfbReceiveLenPos + 1]) << 8) |
        response[esfbReceiveLenPos]);

    co_return true;
}

/*
 * Response packet format:
 * |1 byte|1 byte|   2 bytes   |1 byte|1 byte|max 245 bytes| 1 byte |
 * |Header|Cmd id|Fragment flag|Lenght| Type |     Data    |Checksum|
 */
sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::esfbReadData(
    uint8_t cmdId, std::vector<uint8_t>& data)
{
    if (data.size() > esfbDataMaxSize)
    {
        lg2::error(
            "ESFB read data: requested data size ({SIZE}) exceeds max limit.",
            "SIZE", data.size());
        co_return false;
    }

    std::vector<uint8_t> request;
    size_t expectedPacketLen = esfbReadStableLength + data.size();
    std::vector<uint8_t> response(expectedPacketLen, 0);

    request.push_back(cmdId);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB read data failed.");
        co_return false;
    }

    if (response.size() < expectedPacketLen)
    {
        lg2::error(
            "ESFB read data: response length too short. Expected: {EXP}, Got: {GOT}",
            "EXP", expectedPacketLen, "GOT", response.size());
        co_return false;
    }

    if (response[esfbHeaderPos] != esfbHeader)
    {
        lg2::error("ESFB read data: response header is invalid.");
        co_return false;
    }

    if (response[esfbLengthPos] != static_cast<uint8_t>(expectedPacketLen))
    {
        lg2::error("ESFB read data: response length is invalid.");
        co_return false;
    }

    if (response.back() != checkSum(response))
    {
        lg2::error("ESFB read data: response checksum is invalid.");
        co_return false;
    }

    using Diff = std::vector<uint8_t>::difference_type;
    std::copy(response.begin() + static_cast<Diff>(esfbReceiveDataPos),
              response.begin() + static_cast<Diff>(esfbReceiveDataPos) +
                  static_cast<Diff>(data.size()),
              data.begin());

    co_return true;
}

sdbusplus::async::task<bool>
    LatticeXO5DSeriesCPLD::checkCurrentRunningImageStatus(
        uint32_t* bitStreamVersion)
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> data;
    std::vector<uint8_t> currentImageStatus;
    uint16_t statusLength = 0;
    size_t retryCnt = 0;
    size_t remainLength = 0;
    size_t dataLength = 0;
    EsfbStatus status = EsfbStatus::defaultStatus;
    static constexpr size_t checkImageStatusRetryCount = 3;
    bool success = false;

    do
    {
        co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

        success = co_await esfbWrite(
            esfbFirstAndLastPacket + esfbFirstPacketNum,
            static_cast<uint8_t>(EsfbCommand::checkCurrentRunningImageStatus),
            request);

        retryCnt++;
    } while (!success && retryCnt < checkImageStatusRetryCount);

    if (!success)
    {
        co_return false;
    }

    retryCnt = 0;
    do
    {
        co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

        success = co_await esfbReadStatus(
            static_cast<uint8_t>(EsfbCommand::getCmdStatusLength), status,
            statusLength);

        retryCnt++;
    } while (status == EsfbStatus::busy && retryCnt < esfbRetryCount);

    if (!success)
    {
        co_return false;
    }

    currentImageStatus.reserve(statusLength);
    remainLength = statusLength;

    while (remainLength > 0)
    {
        dataLength =
            (remainLength <= esfbDataMaxSize) ? remainLength : esfbDataMaxSize;

        data.resize(dataLength);

        success = co_await esfbReadData(
            static_cast<uint8_t>(EsfbCommand::getData), data);

        if (!success)
        {
            lg2::error("Failed to read current image status data");
            co_return false;
        }

        currentImageStatus.insert(currentImageStatus.end(), data.begin(),
                                  data.end());
        remainLength -= dataLength;
    }

    if (currentImageStatus.size() < sizeof(CustomerImageStatus))
    {
        lg2::error(
            "Read current image status size ({GOT}) is smaller than required ({REQ}).",
            "GOT", currentImageStatus.size(), "REQ",
            sizeof(CustomerImageStatus));
        co_return false;
    }

    CustomerImageStatus imageStatus;
    std::memcpy(&imageStatus, currentImageStatus.data(),
                sizeof(CustomerImageStatus));

    nonActiveImageId =
        (imageStatus.imageId == static_cast<uint32_t>(ImageId::id1))
            ? ImageId::id2
            : ImageId::id1;

    lg2::info("Active image id: {ID}, Non-active image id: {NON_ACTIVE}", "ID",
              static_cast<uint32_t>(imageStatus.imageId), "NON_ACTIVE",
              static_cast<uint32_t>(nonActiveImageId));

    if (bitStreamVersion != nullptr)
    {
        *bitStreamVersion = imageStatus.bitstreamVersion;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::eraseNonActiveImage()
{
    if (pNormalKeyBlob == nullptr || normalKeyBlobSize == 0)
    {
        lg2::error("Erase aborted: Normal Key Blob is null or size is 0.");
        co_return false;
    }

    std::vector<uint8_t> data;
    size_t remainNormalKeyBlobSize = normalKeyBlobSize;
    const uint8_t* keyBlobOffset = pNormalKeyBlob;
    size_t retryCnt = 0;
    size_t dataLength = 0;
    uint16_t fragmentFlag = esfbFirstPacket + esfbFirstPacketNum;
    uint16_t statusLength = 0;
    bool success = true;
    static constexpr size_t eraseNonActiveImageRetryCount = 3;
    EsfbStatus status = EsfbStatus::defaultStatus;

    while (remainNormalKeyBlobSize > 0)
    {
        if (remainNormalKeyBlobSize <= esfbPackageLen)
        {
            fragmentFlag |= esfbLastPacket;
            dataLength = remainNormalKeyBlobSize;
        }
        else
        {
            dataLength = esfbPackageLen;
        }

        data.clear();
        data.reserve(packetImageIdSize + dataLength);
        data.push_back(static_cast<uint8_t>(nonActiveImageId));
        data.insert(data.end(), keyBlobOffset, keyBlobOffset + dataLength);

        retryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbWrite(
                fragmentFlag,
                static_cast<uint8_t>(EsfbCommand::customerImageErase), data);

            retryCnt++;
        } while (!success && retryCnt < eraseNonActiveImageRetryCount);

        if (!success)
        {
            lg2::error("Failed to write erase chunk. Remaining size: {REMAIN}",
                       "REMAIN", remainNormalKeyBlobSize);
            co_return false;
        }

        retryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(EsfbCommand::getCmdStatusLength), status,
                statusLength);

            retryCnt++;
        } while (!success && retryCnt < esfbRetryCount);

        if (!success || status != EsfbStatus::success)
        {
            lg2::error(
                "Timeout or error waiting for erase status. Remaining size: {REMAIN}, Status: {STATUS}",
                "REMAIN", remainNormalKeyBlobSize, "STATUS",
                static_cast<uint8_t>(status));
            co_return false;
        }

        // Process next packet
        keyBlobOffset += dataLength;
        remainNormalKeyBlobSize -= dataLength;

        fragmentFlag &= ~esfbFirstPacket;
        fragmentFlag += 1;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::programCustomerImage()
{
    if (pCustomerImage == nullptr || customerImageSize == 0)
    {
        lg2::error("Program aborted: Customer image is null or size is 0.");
        co_return false;
    }

    size_t remainImageSize = customerImageSize;
    const uint8_t* remainImageOffset = pCustomerImage;
    size_t retryCnt = 0;
    size_t dataLength = 0;
    uint16_t fragmentFlag = esfbFirstPacket + esfbFirstPacketNum;
    uint16_t statusLength = 0;
    static constexpr size_t programCustomerImageRetryCount = 3;
    std::vector<uint8_t> data;
    bool success = true;
    EsfbStatus status = EsfbStatus::defaultStatus;

    while (remainImageSize > 0)
    {
        if (remainImageSize <= esfbPackageLen)
        {
            fragmentFlag |= esfbLastPacket;
            dataLength = remainImageSize;
        }
        else
        {
            dataLength = esfbPackageLen;
        }

        data.clear();
        data.reserve(packetImageIdSize + dataLength);
        data.push_back(static_cast<uint8_t>(nonActiveImageId));
        data.insert(data.end(), remainImageOffset,
                    remainImageOffset + dataLength);

        retryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbWrite(
                fragmentFlag,
                static_cast<uint8_t>(EsfbCommand::customerImageProgram), data);

            retryCnt++;
        } while (!success && retryCnt < programCustomerImageRetryCount);

        if (!success)
        {
            lg2::error(
                "Failed to write program chunk. Remaining size: {REMAIN}",
                "REMAIN", remainImageSize);
            co_return false;
        }

        retryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(EsfbCommand::getCmdStatusLength), status,
                statusLength);

            retryCnt++;
        } while (!success && retryCnt < programCustomerImageRetryCount);

        if (!success || status != EsfbStatus::success)
        {
            lg2::error(
                "Failed to program image. Remaining size: {REMAIN}, Status: {STATUS}",
                "REMAIN", remainImageSize, "STATUS",
                static_cast<uint8_t>(status));
            co_return false;
        }

        // Process next packet
        remainImageOffset += dataLength;
        remainImageSize -= dataLength;

        fragmentFlag &= ~esfbFirstPacket;
        fragmentFlag += 1;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::dryRunCustomerImage()
{
    std::vector<uint8_t> request;
    uint16_t statusLength = 0;
    size_t statusRetryCnt = 0;
    size_t dryRunRetryCnt = 0;
    bool success = false;
    EsfbStatus status = EsfbStatus::defaultStatus;

    request.reserve(packetImageIdSize);
    request.push_back(static_cast<uint8_t>(nonActiveImageId));

    while (dryRunRetryCnt < esfbRetryCount)
    {
        success = co_await esfbWrite(
            esfbFirstAndLastPacket + esfbFirstPacketNum,
            static_cast<uint8_t>(EsfbCommand::customerImageDryRun), request);

        if (!success)
        {
            lg2::error("Failed to send customer image dry run command.");
            co_return false;
        }

        statusRetryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(EsfbCommand::getCmdStatusLength), status,
                statusLength);

            statusRetryCnt++;
        } while (status == EsfbStatus::busy && statusRetryCnt < esfbRetryCount);

        if (status != EsfbStatus::dryRun)
        {
            break;
        }

        dryRunRetryCnt++;
    }

    if (!success || status != EsfbStatus::success)
    {
        lg2::error("Dry run process failed or aborted. CPLD Status: {STATUS}",
                   "STATUS", static_cast<uint8_t>(status));
        co_return false;
    }

    co_return success;
}

sdbusplus::async::task<bool>
    LatticeXO5DSeriesCPLD::getDryRunCustomerImageResult()
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> dryRunResult;
    uint16_t statusLength = 0;
    size_t statusRetryCnt = 0;
    size_t getDryRunRetryCnt = 0;
    bool success = false;
    EsfbStatus status = EsfbStatus::defaultStatus;

    request.reserve(packetImageIdSize);
    request.push_back(static_cast<uint8_t>(nonActiveImageId));

    // Takes time to get dry-run results.
    while (getDryRunRetryCnt < esfbRetryCount)
    {
        success = co_await esfbWrite(
            esfbFirstAndLastPacket + esfbFirstPacketNum,
            static_cast<uint8_t>(EsfbCommand::getCustomerImageDryRunResult),
            request);

        if (!success)
        {
            lg2::error("Failed to send get dry run result command.");
            co_return false;
        }

        statusRetryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(EsfbCommand::getCmdStatusLength), status,
                statusLength);

            statusRetryCnt++;
        } while (status == EsfbStatus::busy && statusRetryCnt < esfbRetryCount);

        if (status != EsfbStatus::dryRun)
        {
            break;
        }

        getDryRunRetryCnt++;
    }

    if (!success || status != EsfbStatus::success)
    {
        lg2::error(
            "CPLD reported error before reading dry run result. Status: {STATUS}",
            "STATUS", static_cast<uint8_t>(status));

        co_return false;
    }

    dryRunResult.resize(statusLength);
    success = co_await esfbReadData(static_cast<uint8_t>(EsfbCommand::getData),
                                    dryRunResult);

    if (!success || dryRunResult.empty() ||
        dryRunResult[0] != dryRunResultSuccess)
    {
        if (!success)
        {
            lg2::error("Failed to read dry run result data payload.");
        }
        else if (dryRunResult.empty())
        {
            lg2::error("Dry run result payload is unexpectedly empty.");
        }
        else
        {
            lg2::error("Dry run result failed. Result code: {CODE}", "CODE",
                       dryRunResult[0]);
        }
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::setPrimaryCustomerImage()
{
    std::vector<uint8_t> request;
    uint16_t statusLength = 0;
    size_t setPrimaryCustomerImageRetryCnt = 0;
    size_t retryCnt = 0;
    bool success = false;
    EsfbStatus status = EsfbStatus::defaultStatus;

    request.reserve(packetImageIdSize);
    request.push_back(static_cast<uint8_t>(nonActiveImageId));

    while (setPrimaryCustomerImageRetryCnt < esfbRetryCount)
    {
        success = co_await esfbWrite(
            esfbFirstAndLastPacket + esfbFirstPacketNum,
            static_cast<uint8_t>(EsfbCommand::setPrimaryImage), request);

        if (!success)
        {
            lg2::error("Failed to send set primary image command.");
            co_return false;
        }

        retryCnt = 0;
        do
        {
            co_await sdbusplus::async::sleep_for(ctx, esfbSleepInterval);

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(EsfbCommand::getCmdStatusLength), status,
                statusLength);

            retryCnt++;
        } while (status == EsfbStatus::busy && retryCnt < esfbRetryCount);

        if (status != EsfbStatus::dryRun)
        {
            break;
        }

        setPrimaryCustomerImageRetryCnt++;
    }

    if (!success || status != EsfbStatus::success)
    {
        lg2::error("Failed to set primary image. CPLD Status: {STATUS}",
                   "STATUS", static_cast<uint8_t>(status));
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::readUserCode(
    uint32_t& userCode)
{
    if (!co_await checkCurrentRunningImageStatus(&userCode))
    {
        lg2::error("Check current running image status failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::prepareUpdate(
    const uint8_t* image, size_t imageSize)
{
    lg2::debug("Check current running image status.");
    if (!co_await checkCurrentRunningImageStatus(nullptr))
    {
        lg2::error("Check current running image status failed.");
        co_return false;
    }

    if (image == nullptr || imageSize < sizeof(CustomerImageData))
    {
        lg2::error("Prepare update failed: Image size is incorrect.");
        co_return false;
    }

    // Set image pointers and sizes
    pCustomerImage = image;
    customerImageSize = imageSize;
    pNormalKeyBlob =
        pCustomerImage + offsetof(CustomerImageData, normalKeyBlob);
    normalKeyBlobSize = sizeof(CustomerImageData::normalKeyBlob);

    lg2::debug("image size: {IMG}, normal keyBlob size: {KEY}", "IMG",
               customerImageSize, "KEY", normalKeyBlobSize);

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::doErase()
{
    lg2::debug("Erase non-active image {ID}.", "ID",
               static_cast<uint8_t>(nonActiveImageId));
    if (!co_await eraseNonActiveImage())
    {
        lg2::error("Erase non-active image failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::doUpdate()
{
    lg2::debug("Program non-active image {ID}.", "ID",
               static_cast<uint8_t>(nonActiveImageId));
    if (!co_await programCustomerImage())
    {
        lg2::error("Program customer image failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DSeriesCPLD::finishUpdate()
{
    lg2::debug("Dry run customer image.");
    if (!co_await dryRunCustomerImage())
    {
        lg2::error("Dry run customer image failed.");
        co_return false;
    }

    lg2::debug("Get dry run customer image result.");
    if (!co_await getDryRunCustomerImageResult())
    {
        lg2::error("Get dry run customer image result failed.");
        co_return false;
    }

    lg2::debug("Set primary customer image.");
    if (!co_await setPrimaryCustomerImage())
    {
        lg2::error("Set primary customer image failed.");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::cpld
