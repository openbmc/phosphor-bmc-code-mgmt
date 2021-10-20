#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    std::string fileName{};

    CLI::App app{"Update the firmware of OpenBMC via USB app"};
    app.add_option("-f,--fileName", fileName,
                   "Get the name of the USB mount folder");

    CLI11_PARSE(app, argc, argv);

    return 0;
}
