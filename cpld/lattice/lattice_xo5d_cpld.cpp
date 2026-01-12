#include "lattice_xo5d_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

namespace oob
{
/* Stable value in OOB command packet format */
constexpr uint8_t header = 0xBC;

/* OOB command length limits */
constexpr uint8_t dataMaxSize = 245;
constexpr uint8_t packageLen = 128;

/* Packet fragment flags */
constexpr uint16_t firstPacket = 0x8000;
constexpr uint16_t lastPacket = 0x4000;
constexpr uint16_t firstAndLastPacket = 0xC000;
constexpr uint8_t firstPacketNum = 1;

/* Misc settings */
constexpr uint8_t sleepInterval = 10;
constexpr uint16_t retryCount = 5000;
constexpr uint8_t imageId1 = 1;
constexpr uint8_t imageId2 = 2;
constexpr uint8_t imageIdSize = 1;
constexpr uint8_t dryRunResultSuccess = 1;

/* Packet layout positions */
constexpr uint8_t headerPos = 0;
constexpr uint8_t lengthPos = 4;
constexpr uint8_t receiveDataPos = 6;
constexpr uint8_t receiveStatusPos = 6;
constexpr uint8_t receiveLenPos = 7;

/* Fixed packet lengths */
constexpr uint8_t readStatusLength = 10;
constexpr uint8_t readStableLength = 7;
constexpr uint8_t writeCmdLength = 6;
} // namespace oob

enum class oobCommand : uint8_t
{
    detectChannel = 0x01,
    connectionDisable = 0x02,
    customerImageErase = 0x03,
    customerImageProgram = 0x04,
    customerImageDryRun = 0x05,
    customerImageGetStatus = 0x06,
    kakRevoke = 0x07,
    jtagPortLock = 0x08,
    jtagApiLock = 0x09,
    authAndZeroization = 0x20,
    authAndNullification = 0x21,
    warmReboot = 0x22,
    readCustomerPolicy = 0x40,
    eraseUfm = 0x41,
    programUfm = 0x42,
    readUfm = 0x43,
    getCustomerImageDryRunResult = 0x44,
    setPrimaryImage = 0x45,
    setCustomerImageValid = 0x46,
    setMinimumVersion = 0x47,
    getSectorLockInfo = 0x48,
    setProvisionDone = 0x49,
    softwareReboot = 0x4A,
    userMsgAudit = 0x4B,
    checkCurrentRunningImageStatus = 0x4C,
    programCustomerPolicy = 0x4D,
    auditAndLockCustomerPolicy = 0x4E,
    authorizeAndEraseCustomerPolicy = 0x4F,
    programCustomerKeys = 0x50,
    auditAndLockCustomerKeys = 0x51,
    authorizeAndEraseCustomerKeys = 0x52,
    readCustomerKeys = 0x53,
    programCustomerCentralLocks = 0x54,
    auditAndLockCustomerCentralLocks = 0x55,
    authorizeAndEraseCustomerCentralLocks = 0x56,
    readCustomerCentralLocks = 0x57,
    lockUnlockUfm = 0x58,
    getDeviceUniqueId = 0x59,
    getMinimumVersion = 0x5A,
    getTraceId = 0x5B,
    eraseUfmSector = 0x5C,
    getCmdStatusLength = 0x98,
    getData = 0x99,
};

namespace
{
struct CustomerImageData
{
    uint8_t signature[96];
    uint8_t reserved1[160];
    uint32_t bitstreamVersion;
    uint8_t normalKeyBlob[360];
    uint8_t reserved2[144];
};

struct CustomerImageStatus
{
    uint32_t imageId;
    uint32_t dsebVersion;
    uint32_t bitstreamVersion;
    uint32_t doneStatus;
    uint32_t cancelStatus;
    uint32_t valid;
    uint32_t corruptInfo;
    uint8_t kak[96];
    uint8_t isk[96];
    uint8_t ecc384Signature[96];
};

} // namespace

uint8_t LatticeXO5DCPLD::checkSum(const std::vector<uint8_t>& vec)
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
sdbusplus::async::task<bool> LatticeXO5DCPLD::esfbWrite(
    uint16_t fragmentFlag, uint8_t cmdId, std::vector<uint8_t>& data)
{
    uint8_t packetLength = oob::writeCmdLength + data.size();
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;

    request.reserve(packetLength);
    request.push_back(oob::header);
    request.push_back(cmdId);
    request.push_back(static_cast<uint8_t>(fragmentFlag));
    request.push_back(static_cast<uint8_t>(fragmentFlag >> 8));
    request.push_back(packetLength);
    request.insert(request.end(), data.begin(), data.end());
    request.push_back(checkSum(request));

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB write failed");
        co_return false;
    }

    co_return true;
}

/*
 * Response packet format:
 * |1 byte|1 byte|   2 bytes   |1 byte|1 byte|1 byte|2 bytes| 1 byte |
 * |Header|Cmd id|Fragment flag|Lenght| Type |status|  Data |Checksum|
 */
sdbusplus::async::task<bool> LatticeXO5DCPLD::esfbReadStatus(
    uint8_t cmdId, oobStatus* status, uint16_t* data)
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> response(oob::readStatusLength, 0);

    request.push_back(cmdId);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB read status failed.");
        co_return false;
    }

    *status = static_cast<oobStatus>(response[oob::receiveStatusPos]);
    if (*status == oobStatus::busy || *status == oobStatus::dryRun)
    {
        co_return false;
    }

    if (response[oob::headerPos] != oob::header)
    {
        lg2::error("ESFB read status: response header is invalid.");
        co_return false;
    }

    if (response[oob::lengthPos] != oob::readStatusLength)
    {
        lg2::error("ESFB read status: response length is invalid.");
        co_return false;
    }

    if (response.back() != checkSum(response))
    {
        lg2::error("ESFB read status: response checksum is invalid.");
        co_return false;
    }

    *data = (response[oob::receiveLenPos + 1] << 8) |
            response[oob::receiveLenPos];

    co_return true;
}

/*
 * Response packet format:
 * |1 byte|1 byte|   2 bytes   |1 byte|1 byte|max 245 bytes| 1 byte |
 * |Header|Cmd id|Fragment flag|Lenght| Type |     Data    |Checksum|
 */
sdbusplus::async::task<bool> LatticeXO5DCPLD::esfbReadData(
    uint8_t cmdId, std::vector<uint8_t>& data)
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> response(oob::readStableLength + data.size(), 0);

    request.push_back(cmdId);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB read data failed.");
        co_return false;
    }

    // Check header
    if (response[oob::headerPos] != oob::header)
    {
        lg2::error("ESFB read data: response header is invalid.");
        co_return false;
    }

    // Check length
    if (response[oob::lengthPos] != (oob::readStableLength + data.size()))
    {
        lg2::error("ESFB read data: response length is invalid.");
        co_return false;
    }

    // Check checksum
    if (response.back() != checkSum(response))
    {
        lg2::error("ESFB read data: response checksum is invalid.");
        co_return false;
    }

    using Diff = std::vector<uint8_t>::difference_type;
    std::copy(response.begin() + static_cast<Diff>(oob::receiveDataPos),
              response.begin() + static_cast<Diff>(oob::receiveDataPos) +
                  static_cast<Diff>(data.size()),
              data.begin());

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::checkCurrentRunningImageStatus(
    uint32_t* bitStreamVersion)
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> data;
    std::vector<uint8_t> currentImageStatus;

    uint16_t statusLength = 0;
    uint16_t retryCnt = 0;
    uint16_t remainLength = 0;
    uint16_t dataLength = 0;

    oobStatus status = oobStatus::defaultStatus;
    constexpr uint8_t checkImageStatusRetryCount = 3;
    bool success = false;

    do
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(oob::sleepInterval));

        success = co_await esfbWrite(
            oob::firstAndLastPacket + oob::firstPacketNum,
            static_cast<uint8_t>(oobCommand::checkCurrentRunningImageStatus),
            request);

        retryCnt++;
    } while (!success && retryCnt < checkImageStatusRetryCount);

    if (!success)
    {
        co_return false;
    }

    do
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(oob::sleepInterval));

        success = co_await esfbReadStatus(
            static_cast<uint8_t>(oobCommand::getCmdStatusLength), &status,
            &statusLength);

        retryCnt++;
    } while (status == oobStatus::busy && retryCnt < oob::retryCount);

    if (!success)
    {
        co_return false;
    }

    currentImageStatus.reserve(statusLength);
    remainLength = statusLength;

    while (remainLength > 0)
    {
        dataLength = (remainLength <= oob::dataMaxSize) ? remainLength
                                                        : oob::dataMaxSize;

        data.resize(dataLength);

        success = co_await esfbReadData(
            static_cast<uint8_t>(oobCommand::getData), data);

        if (!success)
        {
            co_return false;
        }

        std::copy(data.begin(), data.end(),
                  std::back_inserter(currentImageStatus));

        remainLength -= dataLength;
    }

    if (currentImageStatus.size() < sizeof(CustomerImageStatus))
    {
        lg2::error("read current image status size is incorrect. {SIZE}",
                   "SIZE", currentImageStatus.size());
        co_return false;
    }

    const auto* imageStatus =
        reinterpret_cast<const CustomerImageStatus*>(currentImageStatus.data());

    nonActiveImageId =
        (imageStatus->imageId == oob::imageId1) ? oob::imageId2 : oob::imageId1;

    lg2::info("active image id: {ID}", "ID", imageStatus->imageId);

    if (bitStreamVersion != nullptr)
    {
        *bitStreamVersion = imageStatus->bitstreamVersion;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::eraseNonActiveImage()
{
    std::vector<uint8_t> data;

    uint32_t remainNormalKeyBlobSize = normalKeyBlobSize;
    const uint8_t* keyBlobOffset = pNormalKeyBlob;
    uint16_t fragmentFlag = oob::firstPacket + oob::firstPacketNum;
    uint16_t statusLength = 0;
    uint16_t retryCnt = 0;
    uint8_t dataLength = 0;

    bool success = false;
    oobStatus status = oobStatus::defaultStatus;

    while (remainNormalKeyBlobSize > 0)
    {
        if (remainNormalKeyBlobSize <= oob::packageLen)
        {
            fragmentFlag |= oob::lastPacket;
            dataLength = static_cast<uint8_t>(remainNormalKeyBlobSize);
        }
        else
        {
            dataLength = oob::packageLen;
        }

        data.clear();
        data.reserve(oob::imageIdSize + dataLength);
        data.push_back(nonActiveImageId);

        std::copy(keyBlobOffset, keyBlobOffset + dataLength,
                  std::back_inserter(data));

        success = co_await esfbWrite(
            fragmentFlag, static_cast<uint8_t>(oobCommand::customerImageErase),
            data);

        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(oob::sleepInterval));

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(oobCommand::getCmdStatusLength), &status,
                &statusLength);

            retryCnt++;
        } while (status == oobStatus::busy && retryCnt < oob::retryCount);

        if (!success)
        {
            co_return false;
        }

        // Process next packet
        keyBlobOffset += dataLength;
        remainNormalKeyBlobSize -= dataLength;

        fragmentFlag &= ~oob::firstPacket;
        fragmentFlag += 1;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::programCustomerImage()
{
    uint32_t remainImageSize = customerImageSize;
    const uint8_t* remainImageOffset = pCustomerImage;
    uint16_t fragmentFlag = oob::firstPacket + oob::firstPacketNum;
    uint16_t statusLength = 0;
    uint16_t retryCnt = 0;
    uint8_t dataLength = 0;

    std::vector<uint8_t> data;
    bool success = false;
    oobStatus status = oobStatus::defaultStatus;

    while (remainImageSize > 0)
    {
        if (remainImageSize <= oob::packageLen)
        {
            fragmentFlag |= oob::lastPacket;
            dataLength = static_cast<uint8_t>(remainImageSize);
        }
        else
        {
            dataLength = oob::packageLen;
        }

        data.clear();
        data.reserve(oob::imageIdSize + dataLength);
        data.push_back(nonActiveImageId);

        std::copy(remainImageOffset, remainImageOffset + dataLength,
                  std::back_inserter(data));

        success = co_await esfbWrite(
            fragmentFlag,
            static_cast<uint8_t>(oobCommand::customerImageProgram), data);

        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(oob::sleepInterval));

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(oobCommand::getCmdStatusLength), &status,
                &statusLength);

            retryCnt++;
        } while (status == oobStatus::busy && retryCnt < oob::retryCount);

        if (!success)
        {
            co_return false;
        }

        // Process next packet
        remainImageOffset += dataLength;
        remainImageSize -= dataLength;

        fragmentFlag &= ~oob::firstPacket;
        fragmentFlag += 1;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::dryRunCustomerImage()
{
    std::vector<uint8_t> request;
    uint16_t statusLength = 0;
    uint16_t statusRetryCnt = 0;
    uint16_t dryRunRetryCnt = 0;
    bool success = false;
    oobStatus status = oobStatus::defaultStatus;

    request.reserve(oob::imageIdSize);
    request.push_back(nonActiveImageId);

    while (dryRunRetryCnt < oob::retryCount)
    {
        statusRetryCnt = 0;

        success = co_await esfbWrite(
            oob::firstAndLastPacket + oob::firstPacketNum,
            static_cast<uint8_t>(oobCommand::customerImageDryRun), request);

        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(oob::sleepInterval));

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(oobCommand::getCmdStatusLength), &status,
                &statusLength);

            statusRetryCnt++;
        } while (status == oobStatus::busy && statusRetryCnt < oob::retryCount);

        if (status != oobStatus::dryRun)
        {
            break;
        }

        dryRunRetryCnt++;
    }

    if (!success)
    {
        co_return false;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::getDryRunCustomerImageResult()
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> result;
    uint16_t statusLength = 0;
    uint16_t statusRetryCnt = 0;
    uint16_t dryRunRetryCnt = 0;
    bool success = false;
    oobStatus status = oobStatus::defaultStatus;

    request.reserve(oob::imageIdSize);
    request.push_back(nonActiveImageId);

    while (dryRunRetryCnt < oob::retryCount)
    {
        statusRetryCnt = 0;

        success = co_await esfbWrite(
            oob::firstAndLastPacket + oob::firstPacketNum,
            static_cast<uint8_t>(oobCommand::getCustomerImageDryRunResult),
            request);

        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(oob::sleepInterval));

            success = co_await esfbReadStatus(
                static_cast<uint8_t>(oobCommand::getCmdStatusLength), &status,
                &statusLength);

            statusRetryCnt++;
        } while (status == oobStatus::busy && statusRetryCnt < oob::retryCount);

        if (status != oobStatus::dryRun)
        {
            break;
        }

        dryRunRetryCnt++;
    }

    if (!success)
    {
        co_return false;
    }

    result.resize(statusLength);
    success = co_await esfbReadData(static_cast<uint8_t>(oobCommand::getData),
                                    result);

    if (!success || result[0] != oob::dryRunResultSuccess)
    {
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::setPrimaryCustomerImage()
{
    std::vector<uint8_t> request;
    uint16_t statusLength = 0;
    uint16_t retryCnt = 0;
    bool success = false;
    oobStatus status = oobStatus::defaultStatus;

    request.reserve(oob::imageIdSize);
    request.push_back(nonActiveImageId);

    success = co_await esfbWrite(
        oob::firstAndLastPacket + oob::firstPacketNum,
        static_cast<uint8_t>(oobCommand::setPrimaryImage), request);

    if (!success)
    {
        co_return false;
    }

    do
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(oob::sleepInterval));

        success = co_await esfbReadStatus(
            static_cast<uint8_t>(oobCommand::getCmdStatusLength), &status,
            &statusLength);

        retryCnt++;
    } while (status == oobStatus::busy && retryCnt < oob::retryCount);

    if (!success)
    {
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::readUserCode(uint32_t& userCode)
{
    if (!co_await checkCurrentRunningImageStatus(&userCode))
    {
        lg2::error("Check current running image status failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::prepareUpdate(
    const uint8_t* image, size_t imageSize)
{
    lg2::info("Check current running image status.");
    if (!co_await checkCurrentRunningImageStatus(nullptr))
    {
        lg2::error("Check current running image status failed.");
        co_return false;
    }

    // Set image pointers and sizes
    pCustomerImage = image;
    customerImageSize = imageSize;
    pNormalKeyBlob =
        pCustomerImage + offsetof(CustomerImageData, normalKeyBlob);
    normalKeyBlobSize = sizeof(CustomerImageData::normalKeyBlob);

    lg2::info("image size: {IMG}, normal keyBlob size: {KEY}", "IMG",
              customerImageSize, "KEY", normalKeyBlobSize);

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::doUpdate()
{
    lg2::info("Erase non-active image {ID}.", "ID", nonActiveImageId);
    if (!co_await eraseNonActiveImage())
    {
        lg2::error("Erase non-active image failed.");
        co_return false;
    }

    lg2::info("Program customer image {ID}.", "ID", nonActiveImageId);
    if (!co_await programCustomerImage())
    {
        lg2::error("Program customer image failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::finishUpdate()
{
    lg2::info("Dry run customer image.");
    if (!co_await dryRunCustomerImage())
    {
        lg2::error("Dry run customer image failed.");
        co_return false;
    }

    lg2::info("Get dry run customer image result.");
    if (!co_await getDryRunCustomerImageResult())
    {
        lg2::error("Get dry run customer image result failed.");
        co_return false;
    }

    lg2::info("Set primary customer image.");
    if (!co_await setPrimaryCustomerImage())
    {
        lg2::error("Set primary customer image failed.");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::cpld
