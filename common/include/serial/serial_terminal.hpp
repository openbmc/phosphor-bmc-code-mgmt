#pragma once

#include "serial_port.hpp"

#include <functional>

namespace phosphor::software::serial
{

class SerialTerminal : public SerialPort
{
  public:
    SerialTerminal(const std::string& chipname, const std::string& port,
                   uint32_t baud, const std::string& prompt) :
        SerialPort(chipname, port, baud), prompt_(prompt)
    {}

  protected:
    static std::string_view lstrip(const std::string_view& str);

    bool executeCmd(
        const char* command, const std::string& response,
        const std::function<bool(const std::string_view)>& handler) const;
    bool readLine(std::string& str) const;

  private:
    std::string prompt_;
};

} // namespace phosphor::software::serial
