#include "serial_terminal.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::serial;

static bool startsWith(const std::string_view& str,
                       const std::string_view& what)
{
    return str.compare(0, what.size(), what) == 0;
}

std::string_view SerialTerminal::lstrip(const std::string_view& str)
{
    auto it = str.begin();

    while (it != str.end() && *it == ' ')
        it++;

    return std::string_view(it, str.end());
}

bool SerialTerminal::executeCmd(
    const char* command, const std::string& response,
    const std::function<bool(const std::string_view)>& handler) const
{
    std::string line;
    bool found = false;
    // Retry two times because our input may be garbled for whatever reason,
    // thus initial command may be not recognized on 1st attempt.
    int retry = 2;

    do
    {
        // Our device talks in text mode, so we're pretending to be a
        // human-operated terminal
        if (!writeStr(command))
            return false;

        do
        {
            if (!readLine(line))
                return false;

            if (!found && startsWith(line, response))
            {
                found = handler(line.substr(response.size()));
            }
        } while (line != prompt_);
    } while (!found && --retry > 0);

    return found;
}

bool SerialTerminal::readLine(std::string& str) const
{
    char c;

    str.clear();
    str.reserve(256);

    do
    {
        if (!readChar(&c))
            return false;

        // The device sends CRLF for line feed, let's filter CRs out for
        // simplicity
        if (c == '\r')
            continue;
        // For more flexibility and simplicity of parsing we strip leftmost
        // spaces
        if (c == ' ' && str.empty())
            continue;
        if (c != '\n')
            str.push_back(c);
        // Command prompt doesn't end with LF, but that's basically EOF
        // Final character of the prompt could be space, so in order to speed up
        // this check we first compare lengths, then last char, then everything
        // else
        if (str.size() == prompt_.size() && c == prompt_.back() &&
            str == prompt_)
            break;
    } while (c != '\n');

    debug("Read line: /{LINE}/", "LINE", str);

    return true;
}
