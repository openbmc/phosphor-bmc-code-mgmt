#include "serial_port.hpp"

#include <fcntl.h>
#include <termios.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

#define SERIAL_COMM_TIMEOUT 30

using namespace phosphor::software::serial;

SerialPort::SerialPort(const std::string& chipname, const std::string& portName,
                       uint32_t baud) :
    name(chipname), port(open(portName.c_str(), O_RDWR))
{
    struct termios t;

    if (port == -1)
        throw std::runtime_error("Failed to open " + portName);

    if (tcgetattr(port, &t) != 0)
    {
        close(port);
        throw std::runtime_error("tcgetattr() failed for " + portName);
    }

    cfmakeraw(&t);
    t.c_iflag &= ~(IGNBRK | IXON | IXOFF | IXANY);
    t.c_cflag = CLOCAL | CREAD | CS8; // Assuming 8n1 for now
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = SERIAL_COMM_TIMEOUT;

    cfsetspeed(&t, baud);

    if (tcsetattr(port, TCSANOW, &t) != 0)
    {
        close(port);
        throw std::runtime_error("tcsetattr() failed for " + portName);
    }
}

bool SerialPort::writeData(const void* data, int len) const
{
    int r = write(port, data, len);

    if (r != len)
    {
        error("{NAME}: Failed to write data to serial port", "NAME", name);
        return false;
    }

    return true;
}

bool SerialPort::readChar(char* c) const
{
    if (read(port, c, 1) != 1)
    {
        error("{NAME}: Failed to read data from serial port", "NAME", name);
        return false;
    }
    return true;
}
