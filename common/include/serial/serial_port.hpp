#pragma once

#include <stdint.h>
#include <unistd.h>

#include <string>

namespace phosphor::software::serial
{

class SerialPort
{
  public:
    SerialPort(const std::string& chipname, const std::string& port,
               uint32_t baud);

    ~SerialPort()
    {
        close(port);
    }

  protected:
    bool writeStr(const std::string& str) const
    {
        return writeData(str.c_str(), str.size());
    }

    bool writeData(const void* data, int len) const;
    bool readChar(char* c) const;

    std::string name;
    int port;
};

} // namespace phosphor::software::serial
