#include "utils.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

namespace utils
{

PHOSPHOR_LOG2_USING;

std::string getService(sdbusplus::bus_t& bus, const std::string& path,
                       const std::string& interface)
{
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_BUSNAME, "GetObject");

    method.append(path);
    method.append(std::vector<std::string>({interface}));

    std::vector<std::pair<std::string, std::vector<std::string>>> response;

    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty())
        {
            error(
                "Empty response from mapper for getting service name: {PATH} {INTERFACE}",
                "PATH", path, "INTERFACE", interface);
            return std::string{};
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Error in mapper method call for ({PATH}, {INTERFACE}: {ERROR}",
              "ERROR", e, "PATH", path, "INTERFACE", interface);
        return std::string{};
    }
    return response[0].first;
}

void setProperty(sdbusplus::bus_t& bus, const std::string& objectPath,
                 const std::string& interface, const std::string& propertyName,
                 const PropertyValue& value)
{
    auto service = getService(bus, objectPath, interface);
    if (service.empty())
    {
        return;
    }

    auto method = bus.new_method_call(service.c_str(), objectPath.c_str(),
                                      "org.freedesktop.DBus.Properties", "Set");
    method.append(interface.c_str(), propertyName.c_str(), value);

    bus.call_noreply(method);
}

void mergeFiles(const std::vector<std::string>& srcFiles,
                const std::string& dstFile)
{
    std::ofstream outFile(dstFile, std::ios::out);
    for (const auto& file : srcFiles)
    {
        std::ifstream inFile;
        inFile.open(file, std::ios_base::in);
        if (!inFile)
        {
            continue;
        }

        inFile.peek();
        if (inFile.eof())
        {
            inFile.close();
            continue;
        }

        outFile << inFile.rdbuf();
        inFile.close();
    }
    outFile.close();
}

namespace internal
{

/* @brief Helper function to build a string from command arguments */
static std::string buildCommandStr(char** args)
{
    std::string command = "";
    for (int i = 0; args[i]; i++)
    {
        command += args[i];
        command += " ";
    }
    return command;
}

std::pair<int, std::string> executeCmd(char** args)
{
    std::array<char, 512> buffer;
    std::string cmd = buildCommandStr(args);
    std::stringstream result;
    int rc;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        error("Failed to execute command: {COMMAND}", "COMMAND", cmd);
        return {-1, std::string{}};
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result << buffer.data();
    }
    rc = pclose(pipe);
    return {rc, result.str()};
}

} // namespace internal

} // namespace utils
