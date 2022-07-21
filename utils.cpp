#include "utils.hpp"

#include <unistd.h>

#include <sstream>
#include <fstream>

#include <phosphor-logging/lg2.hpp>

namespace utils
{

PHOSPHOR_LOG2_USING;

//inline
size_t offset(const char* buf, size_t len, const char* str)
{
    return std::search(buf, buf + len, str, str + strlen(str)) - buf;
}

std::string getMtdDev(std::string name)
{
    std::ifstream infile("/proc/mtd");
    std::string match = "\"" + name + "\"";

    if (infile)
    {
        std::string line;
        while (std::getline(infile, line))
        {
            std::istringstream iss(line);
            std::string f1,f2,f3,f4;
            if (!(iss >> f1 >> f2 >> f3 >> f4)) { break; } // error
            if (f4 == match)
            {
                f1.pop_back();
                return f1;
            }
        }
    }
    return "";
}

int parseVersion(std::string& property, std::string& version)
{
    std::size_t pos;
    std::map<int, std::string> hostPropertyData;
    std::string line;
    std::stringstream input(property);
    char buf[255] = {0};

    std::getline(input, line); // Get magic word
    for(int i = 0; i < TOTAL; i++)
    {
        std::getline(input, line);
        pos = line.find(_hostPropertyMap.at(i));

        if (pos != std::string::npos)
        {
            hostPropertyData[i] = line.substr(_hostPropertyMap.at(i).size());
            hostPropertyData[i].erase(hostPropertyData[i].end()-1, hostPropertyData[i].end()); // Remove the last null
        }
        else
        {
            error("Host information incorrect!");
            return 0;
        } 
    }

    // Compose Host Version Information
    snprintf(buf, 255, "%s v%s.%s (%s/%s/%s)",
            hostPropertyData[FAMILY].data(), hostPropertyData[MAJOR].data(),
            hostPropertyData[MINOR].data(), hostPropertyData[DAY].data(),
            hostPropertyData[MONTH].data(), hostPropertyData[YEAR].data());
    version = buf;

    return 1;
}

std::string getHostVersion() 
{
    std::ifstream efile;
    std::string property;
    std::string version("unknown-0\nProduct-unknown-0");
    char buffer[BUF_SIZE];

    std::string mtddev = getMtdDev("host-prime");
    if (mtddev == "")
    {
        return version;
    }
    mtddev = "/dev/" + mtddev;
    try
    {
    efile.open(mtddev);
    efile.seekg(PROPERTY_OFFSET, std::ios::beg);
    efile.read(buffer, BUF_SIZE);
    efile.close();
    }
    catch (const std::exception& e)
    {
    if (!efile.eof())
    {
        error("Error in reading host information");
    }
    efile.close();
    }

    size_t propOffset = offset(buffer, BUF_SIZE, MAGIC_WORD);
    if (propOffset != BUF_SIZE) 
    { // Found magic word
        std::string property(buffer+propOffset);
        if (!parseVersion(property, version)) 
        {
            version = "unknown-0\nProduct-unknown-0";
        }
    }

return version;
}

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
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
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error in mapper method call for ({PATH}, {INTERFACE}: {ERROR}",
              "ERROR", e, "PATH", path, "INTERFACE", interface);
        return std::string{};
    }
    return response[0].first;
}

void setProperty(sdbusplus::bus::bus& bus, const std::string& objectPath,
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
static std::string buildCommandStr(const char* name, char** args)
{
    std::string command = name;
    for (int i = 0; args[i]; i++)
    {
        command += " ";
        command += args[i];
    }
    return command;
}

int executeCmd(const char* path, char** args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execv(path, args);

        // execv only retruns on err
        auto err = errno;
        auto command = buildCommandStr(path, args);
        error("Failed ({ERRNO}) to execute command: {COMMAND}", "ERRNO", err,
              "COMMAND", command);
        return -1;
    }
    else if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            error("Error ({ERRNO}) during waitpid.", "ERRNO", errno);
            return -1;
        }
        else if (WEXITSTATUS(status) != 0)
        {
            auto command = buildCommandStr(path, args);
            error("Error ({STATUS}) occurred when executing command: {COMMAND}",
                  "STATUS", status, "COMMAND", command);
            return -1;
        }
    }
    else
    {
        error("Error ({ERRNO}) during fork.", "ERRNO", errno);
        return -1;
    }

    return 0;
}

} // namespace internal

int flashEraseMTD(const std::string& mtdName)
{
std::string mtdDevice = getMtdDev(mtdName);
int rc = -1;

if (mtdDevice == "")
{
    error("Failed to match mtd device: {DEV_NAME}", "DEV_NAME", mtdName.c_str());
    return rc;
}

// Erase mtd device by flash_eraseall command
std::string cmd = "flash_eraseall /dev/" + mtdDevice;
std::array<char, 512> buffer;
std::stringstream result;

FILE* pipe = popen(cmd.c_str(), "r");
if (!pipe)
{
    throw std::runtime_error("popen() failed!");
}
while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
{
    result << buffer.data();
}
rc = pclose(pipe);

return rc;
}
} // namespace utils
