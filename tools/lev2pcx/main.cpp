#include "lev_reader.h"
#include "pcx_writer.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: lev2pcx <input.LEV> <output.PCX>\n";
        return 2;
    }

    const std::filesystem::path inputPath(argv[1]);
    const std::filesystem::path outputPath(argv[2]);
    auto level = std::make_unique<Level>();
    std::string error;

    if (!loadLev(inputPath, *level, error)) {
        std::cerr << "lev2pcx: " << inputPath.string() << ": " << error << '\n';
        return 1;
    }
    if (!savePcx(outputPath, *level, error)) {
        std::cerr << "lev2pcx: " << outputPath.string() << ": " << error << '\n';
        return 1;
    }

    std::cout << "Exported \"" << level->name << "\" to "
              << outputPath.string() << '\n';
    return 0;
}
