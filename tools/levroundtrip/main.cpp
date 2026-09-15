#include "lev_reader.h"
#include "lev_writer.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: levroundtrip <input.LEV> <output.LEV> [level-name]\n";
        return 2;
    }

    const std::filesystem::path inputPath(argv[1]);
    const std::filesystem::path outputPath(argv[2]);
    auto level = std::make_unique<Level>();
    std::string error;

    if (!loadLev(inputPath, *level, error)) {
        std::cerr << "levroundtrip: " << inputPath.string() << ": " << error
                  << '\n';
        return 1;
    }
    if (argc == 4) {
        level->name = argv[3];
    }
    if (!saveLev(outputPath, *level, error)) {
        std::cerr << "levroundtrip: " << outputPath.string() << ": " << error
                  << '\n';
        return 1;
    }

    std::cout << "Saved \"" << level->name << "\" to "
              << outputPath.string() << '\n';
    return 0;
}
