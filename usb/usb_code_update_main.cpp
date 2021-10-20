#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    std::string usbPath{};

    CLI::App app{"Update the firmware of OpenBMC via USB app"};
    app.add_option("-p,--path", usbPath, "Get the USB mount path");

    CLI11_PARSE(app, argc, argv);

    return 0;
}
