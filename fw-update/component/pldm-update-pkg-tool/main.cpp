#include "create.hpp"
#include "info.hpp"

#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <string>

// tool to create and view pldm fw update package

void printHelp()
{
    std::cout << "--help            print help" << std::endl;
    printHelpPkgInfo();
    printHelpCreateUpdatePkg();
}

int main(int argc, char* argv[])
{
    bool create = false;
    bool help = false;
    bool info = false;

    for (int i = 0; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--help")
        {
            help = true;
        }
        if (arg == "--create")
        {
            create = true;
        }
        if (arg == "--info")
        {
            info = true;
        }
    }

    if (help)
    {
        printHelp();
        return 0;
    }

    if (create)
    {
        return createUpdatePkg(argc - 2, argv + 2);
    }

    if (info)
    {
        return printPkgInfo(argc - 2, argv + 2);
    }

    std::cout << "please select an option" << std::endl;

    printHelp();

    return 1;
}
