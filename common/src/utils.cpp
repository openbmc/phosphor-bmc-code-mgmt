#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

PHOSPHOR_LOG2_USING;

sdbusplus::async::task<bool> asyncSystem(
    sdbusplus::async::context& ctx, const std::string& cmd,
    std::optional<std::reference_wrapper<std::string>> result)
{
    int exitPipefd[2];
    int resultPipefd[2];

    if (pipe(exitPipefd) == -1 || (result && pipe(resultPipefd) == -1))
    {
        error("Failed to create pipe for command: {CMD}", "CMD", cmd);
        co_return false;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(exitPipefd[0]);

        if (result)
        {
            close(resultPipefd[0]);
            dup2(resultPipefd[1], STDOUT_FILENO);
            dup2(resultPipefd[1], STDERR_FILENO);
            close(resultPipefd[1]);
        }

        int exitCode = std::system(cmd.c_str());
        ssize_t status = write(exitPipefd[1], &exitCode, sizeof(exitCode));

        close(exitPipefd[1]);
        _exit((status == sizeof(exitCode)) ? 0 : 1);
    }
    else if (pid > 0)
    {
        close(exitPipefd[1]);

        if (result)
        {
            close(resultPipefd[1]);
        }

        auto fdio =
            std::make_unique<sdbusplus::async::fdio>(ctx, exitPipefd[0]);

        if (!fdio)
        {
            error("Failed to create fdio for command: {CMD}", "CMD", cmd);
            close(exitPipefd[0]);
            co_return false;
        }

        co_await fdio->next();

        if (result)
        {
            auto& resStr = result->get();
            resStr.clear();
            char buffer[1024];
            ssize_t n;
            while ((n = read(resultPipefd[0], buffer, sizeof(buffer))) > 0)
            {
                resStr.append(buffer, n);
            }
            close(resultPipefd[0]);
        }

        int exitCode = -1;
        ssize_t bytesRead = read(exitPipefd[0], &exitCode, sizeof(exitCode));
        close(exitPipefd[0]);

        if (bytesRead != sizeof(exitCode))
        {
            error("Failed to read exit code from command {CMD}", "CMD", cmd);
            co_return false;
        }

        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            error("waitpid failed for PID {PID} for command {CMD}", "PID", pid,
                  "CMD", cmd);
            co_return false;
        }

        if (exitCode != 0)
        {
            error("Command {CMD} exited with code {CODE}", "CMD", cmd, "CODE",
                  exitCode);
            co_return false;
        }

        debug("{CMD} executed successfully", "CMD", cmd);

        co_return true;
    }
    else
    {
        error("Fork failed for command: {CMD}", "CMD", cmd);
        close(exitPipefd[0]);
        close(exitPipefd[1]);
        if (result)
        {
            close(resultPipefd[0]);
            close(resultPipefd[1]);
        }
        co_return false;
    }
}

std::string trim(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    {
        ++b;
    }

    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    {
        --e;
    }

    return s.substr(b, e - b);
}

bool parseHexByte(const std::string& input, uint8_t& value)
{
    std::string s = trim(input);

    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
    {
        s = s.substr(2);
    }

    if (s.empty() || s.size() > 2)
    {
        return false;
    }

    unsigned int v = 0;
    std::stringstream ss;
    ss << std::hex << s;
    ss >> v;

    if (ss.fail() || v > 0xFF)
    {
        return false;
    }

    value = static_cast<uint8_t>(v);
    return true;
}

bool parseHexBytes(const std::string& input, std::vector<uint8_t>& out)
{
    std::string s = trim(input);

    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
    {
        s = s.substr(2);
    }

    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char c) { return std::isspace(c); }),
            s.end());

    if (s.empty() || (s.size() % 2) != 0)
    {
        return false;
    }

    out.clear();

    for (size_t i = 0; i < s.size(); i += 2)
    {
        std::string byteStr = s.substr(i, 2);

        unsigned int v = 0;
        std::stringstream ss;
        ss << std::hex << byteStr;
        ss >> v;

        if (ss.fail() || v > 0xFF)
        {
            return false;
        }

        out.push_back(static_cast<uint8_t>(v));
    }

    return true;
}

std::string byteToHex(uint8_t b)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(2)
        << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

std::string bytesToHex(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
    {
        return "-";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
        {
            oss << " ";
        }

        oss << byteToHex(bytes[i]);
    }

    return oss.str();
}
