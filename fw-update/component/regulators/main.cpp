#include "fw-update/component/regulators/vr.hpp"

int main(int argc, char* argv[])
{

    uint8_t bus;
    uint8_t addr;
    std::string type = "XDPE152C4";

    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--type" && i < argc - 1)
        {
            type = std::string(argv[i+1]);
        }
        if (arg == "--bus")
        {
            bus = std::stoul(std::string(argv[i+1],2,10));
        }
        if (arg == "--addr")
        {
            addr = std::stoul(std::string(argv[i+1],2,10));
        }
    }
    std::unique_ptr<VR::VoltageRegulator> dev = VR::create(type, bus, addr);

    std::cout << "Bus: " << bus << std::endl;
    std::cout << "Address: " << addr << std::endl;
    std::cout << "Type: " << type << std::endl;


    return 0;
}
