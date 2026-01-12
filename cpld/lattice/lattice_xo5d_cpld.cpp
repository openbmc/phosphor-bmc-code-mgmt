#include "lattice_xo5d_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

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
    uint8_t packetLength = OOB_WRITE_CMD_LENGTH + data.size();
    std::vector<uint8_t> request;
    std::vector<uint8_t> response;

    request.reserve(packetLength);
    request.push_back(OOB_HEADER);
    request.push_back(cmdId);
    request.push_back((uint8_t)fragmentFlag);
    request.push_back((uint8_t)(fragmentFlag >> 8));
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
    std::vector<uint8_t> response(OOB_READ_STATUS_LENGTH, 0);

    request.push_back(cmdId);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB read status failed.");
        co_return false;
    }

    *status = (oobStatus)response[RECEIVE_STATUS_POS];
    if (*status == oobStatus::STATUS_BUSY ||
        *status == oobStatus::STATUS_DRY_RUN)
    {
        co_return false;
    }

    // check header
    if (response[HEADER_POS] != OOB_HEADER)
    {
        lg2::error("ESFB read status: response header is invalid.");
        co_return false;
    }

    // check length
    if (response[LENGTH_POS] != OOB_READ_STATUS_LENGTH)
    {
        lg2::error("ESFB read status: response length is invalid.}");
        co_return false;
    }

    // check ckhecsum
    if (response.back() != checkSum(response))
    {
        lg2::error("ESFB read status: response checksum is invalid.");
        co_return false;
    }

    *data = (response[RECEIVE_LEN_POS + 1] << 8) | response[RECEIVE_LEN_POS];
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
    std::vector<uint8_t> response(OOB_READ_STABLE_LENGTH + data.size(), 0);

    request.push_back(cmdId);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("ESFB read data failed.");
        co_return false;
    }

    // check header
    if (response[HEADER_POS] != OOB_HEADER)
    {
        lg2::error("ESFB read data: response header is invalid.");
        co_return false;
    }

    // check length
    if (response[LENGTH_POS] != (OOB_READ_STABLE_LENGTH + data.size()))
    {
        lg2::error("ESFB read data: response length is invalid.}");
        co_return false;
    }

    // check ckhecsum
    if (response.back() != checkSum(response))
    {
        lg2::error("ESFB read data: response checksum is invalid.}");
        co_return false;
    }

    using Diff = std::vector<uint8_t>::difference_type;
    std::copy(response.begin() + static_cast<Diff>(RECEIVE_DATA_POS),
              response.begin() + static_cast<Diff>(RECEIVE_DATA_POS) +
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
    uint16_t statusLength;
    uint16_t retryCnt = 0;
    uint16_t remainLength;
    uint16_t dataLength;
    oobStatus status = oobStatus::STATUS_DEFAULT;
    static const uint8_t checkImageStatusRetryCount = 3;
    auto success = false;

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL));
        success =
            co_await esfbWrite((FIRST_AND_LAST_PACKET + FIRST_PACKET_NUM),
                               OOB_CHECK_CURRENT_RUNNING_IMAGE_STATUS, request);
        retryCnt++;
    } while (!success && retryCnt < checkImageStatusRetryCount);

    if (!success)
    {
        co_return false;
    }

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL));
        success = co_await esfbReadStatus(OOB_GET_CMD_STATUS_LENGTH, &status,
                                          &statusLength);
        retryCnt++;
    } while (status == oobStatus::STATUS_BUSY && retryCnt < RETRY_COUNT);

    if (!success)
    {
        co_return false;
    }

    currentImageStatus.reserve(statusLength);
    remainLength = statusLength;

    while (remainLength)
    {
        dataLength =
            (remainLength <= DATA_MAX_SIZE) ? remainLength : DATA_MAX_SIZE;

        data.resize(dataLength);
        success = co_await esfbReadData(OOB_GET_DATA, data);
        std::copy(data.begin(), data.end(),
                  std::back_inserter(currentImageStatus));

        remainLength -= dataLength;
    }

    if (currentImageStatus.size() < sizeof(customerImageStatus_t))
    {
        lg2::error("read current image status size is incorrect. {SIZE} ",
                   "SIZE", currentImageStatus.size());
        co_return false;
    }

    pCustomerImageStatus pImageStatus =
        reinterpret_cast<pCustomerImageStatus>(currentImageStatus.data());

    nonActiveImageId =
        (pImageStatus->imageId == IMAGE_ID_1) ? IMAGE_ID_2 : IMAGE_ID_1;
    lg2::info("non-active image id: {ID}", "ID", nonActiveImageId);

    if (bitStreamVersion)
    {
        *bitStreamVersion = pImageStatus->bitstreamVersion;
    }

    lg2::info("bitstreamVersion: {ID}", "ID", pImageStatus->bitstreamVersion);

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::eraseNonActiveImage()
{
    std::vector<uint8_t> data;
    uint32_t remainNormalKeyBlobSize = normalKeyBlobSize;
    const uint8_t* pKeyBlobOffset = pNormalKeyBlob;
    uint16_t fragmentFlag;
    uint16_t statusLength;
    uint16_t retryCnt = 0;
    uint8_t dataLength;
    auto success = false;
    oobStatus status = oobStatus::STATUS_DEFAULT;

    fragmentFlag = FIRST_PACKET + FIRST_PACKET_NUM;

    while (remainNormalKeyBlobSize)
    {
        if (remainNormalKeyBlobSize <= PACKAGE_LEN)
        {
            fragmentFlag |= LAST_PACKET;
            dataLength = remainNormalKeyBlobSize;
        }
        else
        {
            dataLength = PACKAGE_LEN;
        }

        data.clear();
        data.reserve(IMAGE_ID_SIZE + dataLength);
        data.push_back(nonActiveImageId);
        std::copy(pKeyBlobOffset, (pKeyBlobOffset + dataLength),
                  std::back_inserter(data));
        success =
            co_await esfbWrite(fragmentFlag, OOB_CUSTOMER_IMAGE_ERASE, data);
        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SLEEP_INTERVAL));
            success = co_await esfbReadStatus(OOB_GET_CMD_STATUS_LENGTH,
                                              &status, &statusLength);
            retryCnt++;
        } while (status == oobStatus::STATUS_BUSY && retryCnt < RETRY_COUNT);

        if (!success)
        {
            co_return false;
        }

        // process next packet
        pKeyBlobOffset += dataLength;
        remainNormalKeyBlobSize -= dataLength;
        fragmentFlag &= ~FIRST_PACKET;
        fragmentFlag += 1;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::programCustomerImage()
{
    uint32_t remainImageSize = customerImageSize;
    const uint8_t* pRemainImageOffset = pCustomerImage;
    uint16_t fragmentFlag;
    uint16_t statusLength;
    uint16_t retryCnt = 0;
    uint8_t dataLength;
    std::vector<uint8_t> data;
    auto success = false;
    oobStatus status = oobStatus::STATUS_DEFAULT;

    fragmentFlag = FIRST_PACKET + FIRST_PACKET_NUM;

    while (remainImageSize)
    {
        if (remainImageSize <= PACKAGE_LEN)
        {
            fragmentFlag |= LAST_PACKET;
            dataLength = remainImageSize;
        }
        else
        {
            dataLength = PACKAGE_LEN;
        }

        data.clear();
        data.reserve(IMAGE_ID_SIZE + dataLength);
        data.push_back(nonActiveImageId);
        std::copy(pRemainImageOffset, (pRemainImageOffset + dataLength),
                  std::back_inserter(data));
        success =
            co_await esfbWrite(fragmentFlag, OOB_CUSTOMER_IMAGE_PROGRAM, data);
        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SLEEP_INTERVAL));
            success = co_await esfbReadStatus(OOB_GET_CMD_STATUS_LENGTH,
                                              &status, &statusLength);
            retryCnt++;
        } while (status == oobStatus::STATUS_BUSY && retryCnt < RETRY_COUNT);

        if (!success)
        {
            co_return false;
        }

        // process next packet
        pRemainImageOffset += dataLength;
        remainImageSize -= dataLength;
        fragmentFlag &= ~FIRST_PACKET;
        fragmentFlag += 1;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::dryRunCustomerImage()
{
    std::vector<uint8_t> request;
    uint16_t statusLength;
    uint16_t statusRetryCnt = 0;
    uint16_t dryRunRetryCnt = 0;
    auto success = false;
    oobStatus status = oobStatus::STATUS_DEFAULT;

    request.reserve(IMAGE_ID_SIZE);
    request.push_back(nonActiveImageId);

    while (dryRunRetryCnt < RETRY_COUNT)
    {
        statusRetryCnt = 0;
        success = co_await esfbWrite((FIRST_AND_LAST_PACKET + FIRST_PACKET_NUM),
                                     OOB_CUSTOMER_IMAGE_DRYRUN, request);
        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SLEEP_INTERVAL));
            success = co_await esfbReadStatus(OOB_GET_CMD_STATUS_LENGTH,
                                              &status, &statusLength);
            statusRetryCnt++;
        } while (status == oobStatus::STATUS_BUSY &&
                 statusRetryCnt < RETRY_COUNT);

        if (status != oobStatus::STATUS_DRY_RUN)
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
    std::vector<uint8_t> dataBuffer;
    std::vector<uint8_t> result;
    uint16_t statusLength;
    uint16_t statusRetryCnt = 0;
    uint16_t dryRunRetryCnt = 0;
    oobStatus status = oobStatus::STATUS_DEFAULT;
    auto success = false;

    request.reserve(IMAGE_ID_SIZE);
    request.push_back(nonActiveImageId);

    while (dryRunRetryCnt < RETRY_COUNT)
    {
        statusRetryCnt = 0;
        success =
            co_await esfbWrite((FIRST_AND_LAST_PACKET + FIRST_PACKET_NUM),
                               OOB_GET_CUSTOMER_IMAGE_DRYRUN_RESULT, request);
        if (!success)
        {
            co_return false;
        }

        do
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SLEEP_INTERVAL));
            success = co_await esfbReadStatus(OOB_GET_CMD_STATUS_LENGTH,
                                              &status, &statusLength);
            statusRetryCnt++;
        } while (status == oobStatus::STATUS_BUSY &&
                 statusRetryCnt < RETRY_COUNT);

        if (status != oobStatus::STATUS_DRY_RUN)
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
    success = co_await esfbReadData(OOB_GET_DATA, result);
    if ((!success) || (result[0] != DRY_RUN_RESULT_SUCCESS))
    {
        co_return false;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::setPrimaryCustomerImage()
{
    std::vector<uint8_t> request;
    uint16_t statusLength;
    uint16_t retryCnt = 0;
    oobStatus status = oobStatus::STATUS_DEFAULT;
    auto success = false;

    request.reserve(IMAGE_ID_SIZE);
    request.push_back(nonActiveImageId);
    success = co_await esfbWrite((FIRST_AND_LAST_PACKET + FIRST_PACKET_NUM),
                                 OOB_SET_PRIMARY_IMAGE, request);
    if (!success)
    {
        co_return false;
    }

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL));
        success = co_await esfbReadStatus(OOB_GET_CMD_STATUS_LENGTH, &status,
                                          &statusLength);
        retryCnt++;
    } while (status == oobStatus::STATUS_BUSY && retryCnt < RETRY_COUNT);

    if (!success)
    {
        co_return false;
    }

    co_return success;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::readUserCode(uint32_t& userCode)
{
    if (!(co_await checkCurrentRunningImageStatus(&userCode)))
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
    if (!(co_await checkCurrentRunningImageStatus(nullptr)))
    {
        lg2::error("Check current running image status failed.");
        co_return false;
    }

    pCustomerImage = image;
    customerImageSize = imageSize;
    pNormalKeyBlob =
        pCustomerImage + offsetof(customerImageData_t, normalKeyBlob);
    normalKeyBlobSize = sizeof(customerImageData_t::normalKeyBlob);

    lg2::info("image size: {IMG}, normal keyBlob size: {KEY}", "IMG",
              customerImageSize, "KEY", normalKeyBlobSize);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::doUpdate()
{
    lg2::info("Erase non-active image.");
    if (!(co_await eraseNonActiveImage()))
    {
        lg2::error("Erase non-active image failed.");
        co_return false;
    }

    lg2::info("Program customer image.");
    if (!(co_await programCustomerImage()))
    {
        lg2::error("Program customer image failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5DCPLD::finishUpdate()
{
    lg2::info("Dry run customer image.");
    if (!(co_await dryRunCustomerImage()))
    {
        lg2::error("Dry run customer image failed.");
        co_return false;
    }

    lg2::info("Get dry run customer image result.");
    if (!(co_await getDryRunCustomerImageResult()))
    {
        lg2::error("Get dry run customer image result failed.");
        co_return false;
    }

    lg2::info("Set primary customer image.");
    if (!(co_await setPrimaryCustomerImage()))
    {
        lg2::error("Set primary customer image failed.");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::cpld
