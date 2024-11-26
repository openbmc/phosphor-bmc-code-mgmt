#include "test/create_package/create_pldm_fw_package.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iostream>
#include <string>

void printHelpCreateUpdatePkg()
{
    std::cout << "--create                         create pldm update package"
              << std::endl;

    std::cout
        << "--component <component-filename>    create pldm update package with that component image"
        << std::endl;
    std::cout
        << "--vendorIANA <vendorIANA>        vendor IANA for firmware device id"
        << std::endl;

    std::cout
        << "--compatible <compatible>        compatible string for firmware device id"
        << std::endl;
};

std::shared_ptr<uint8_t[]> readFileToBuffer(const std::string& filename,
                                            size_t* size_out)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    *size_out = size;

    auto buffer = std::make_shared<uint8_t[]>(size);

    if (!file.read(reinterpret_cast<char*>(buffer.get()), size))
    {
        lg2::error("Error reading file: {FILENAME}", "FILENAME", filename);
        return nullptr;
    }

    return buffer;
}

int createUpdatePkg(int argc, char* argv[])
{
    std::shared_ptr<uint8_t[]> sample_image = std::make_shared<uint8_t[]>(3);
    std::shared_ptr<uint8_t[]> sh = sample_image;
    size_t size = 3;

    std::optional<uint32_t> optVendorIANA = std::nullopt;
    std::optional<std::string> optCompatible = std::nullopt;

    for (int i = 0; i < argc - 1; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--vendorIANA")
        {
            optVendorIANA = std::stoll(argv[i + 1], nullptr, 0);
            i++;
        }

        if (arg == "--compatible")
        {
            optCompatible = std::string(argv[i + 1]);
            i++;
        }

        if (arg == "--component")
        {
            std::string filename(argv[i + 1]);
            sh = readFileToBuffer(filename, &size);
            i++;
        }
    }

    if (sh == sample_image)
    {
        lg2::info("no component image file provided, fallback to sample data");
    }

    size_t size_out;
    auto shptr = create_pldm_package_buffer(sh.get(), size, optVendorIANA,
                                            optCompatible, size_out);

    std::string filenameOut = "pldm-package.bin";

    std::ofstream of(filenameOut);
    if (!of.good())
    {
        return 1;
    }

    of.write(reinterpret_cast<char*>(shptr.get()), (long)size_out);

    of.close();

    lg2::info("created {FILENAME}", "FILENAME", filenameOut);

    return 0;
}
