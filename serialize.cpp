#include "config.h"
#include <experimental/filesystem>
#include <cereal/archives/json.hpp>
#include <fstream>
#include "serialize.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

void storeToFile(std::string versionId, uint8_t priority)
{
    if(!fs::is_directory(PERSIST_DIR))
    {
        fs::create_directory(PERSIST_DIR);
    }
    std::string path = PERSIST_DIR + versionId;

    std::ofstream os(path.c_str());
    cereal::JSONOutputArchive oarchive(os);
    oarchive(cereal::make_nvp("priority", priority));
}

void restoreFromFile(std::string versionId, uint8_t& priority)
{
    std::string path = PERSIST_DIR + versionId;
    if (fs::exists(path))
    {
        std::ifstream is(path.c_str(), std::ios::in);
        cereal::JSONInputArchive iarchive(is);
        iarchive(cereal::make_nvp("priority", priority));
    }
}

void removeFile(std::string versionId)
{
    std::string path = PERSIST_DIR + versionId;
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

} // namespace phosphor
} // namespace software
} // namespace openpower
