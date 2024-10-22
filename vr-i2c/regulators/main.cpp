#include "vr.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    int bus = 0;
    int addr = 0;
    std::string type = "XDPE152XX";

    for (int i = 1; i < argc; i++)
    {
        std::cout << "Parsing round: " << std::string(argv[i]) << std::endl;
        std::string arg = std::string(argv[i]);
        if (arg == "--type" && i < argc - 1)
        {
            type = std::string(argv[i + 1]);
            std::cout << "Type: " << type  << std::endl;
            i++;
        }
        if (arg == "--bus" && i < argc - 1)
        {
            bus = std::atoi(argv[i+1]);
            std::cout << "Bus: " << bus  << std::endl;
            i++;
        }
        if (arg == "--addr" && i < argc -1)
        {
            addr = std::atoi(argv[i+1]);
            std::cout << "Addr: " << addr  << std::endl;
            i++;
        }
    }

    std::cout << "Parsing done" << std::endl;

    std::unique_ptr<VR::VoltageRegulator> dev = VR::create(type, bus, addr);

    std::cout << "VR created" << std::endl;

    uint8_t devId[2] = {0};
    if (dev->get_device_id(devId) < 0 )
    {
        return -1;
    }

    std::cout << "Bus: " << bus << std::endl;
    std::cout << "Address: " << addr << std::endl;
    std::cout << "Type: " << type << std::endl;
    std::cout << "VR DevId: 0x" << std::hex << unsigned(devId[0]) << std::endl;

    uint8_t remain = 0;

    if (dev->get_remaining_writes(&remain) < 0)
    {
        return -1;
    }

    std::cout << "Remaining writes: " << unsigned(remain) << std::endl;

    return 0;
}
