#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open file");
    }

    const std::streampos end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("could not determine file size");
    }

    const auto size = static_cast<std::uintmax_t>(end);
    if (size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("file is too large to read into memory");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);

    if (!bytes.empty()) {
        if (bytes.size() > static_cast<std::size_t>(
                               std::numeric_limits<std::streamsize>::max())) {
            throw std::runtime_error("file is too large for the input stream");
        }

        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read the complete file");
        }
    }

    return bytes;
}

void printOffset(const std::size_t offset)
{
    std::cout << "0x" << std::hex << std::uppercase << std::setw(8)
              << std::setfill('0') << offset << std::dec << std::nouppercase
              << std::setfill(' ');
}

void printByte(const std::uint8_t byte)
{
    std::cout << "0x" << std::hex << std::uppercase << std::setw(2)
              << std::setfill('0') << static_cast<unsigned int>(byte)
              << std::dec << std::nouppercase << std::setfill(' ');
}

void printFirstDifference(const std::vector<std::uint8_t>& first,
                          const std::vector<std::uint8_t>& second)
{
    const std::size_t commonSize = std::min(first.size(), second.size());
    std::size_t offset = 0;
    while (offset < commonSize && first[offset] == second[offset]) {
        ++offset;
    }

    std::cout << "First difference: ";
    if (offset < commonSize) {
        printOffset(offset);
        std::cout << "  A=";
        printByte(first[offset]);
        std::cout << "  B=";
        printByte(second[offset]);
        std::cout << '\n';
    } else if (first.size() != second.size()) {
        printOffset(commonSize);
        std::cout << "  A=";
        if (commonSize < first.size()) {
            printByte(first[commonSize]);
        } else {
            std::cout << "<EOF>";
        }
        std::cout << "  B=";
        if (commonSize < second.size()) {
            printByte(second[commonSize]);
        } else {
            std::cout << "<EOF>";
        }
        std::cout << '\n';
    } else {
        std::cout << "none (files are identical)\n";
    }
}

void printRange(const char* kind, const std::size_t start,
                const std::size_t end)
{
    std::cout << std::left << std::setw(10) << kind << std::right;
    printOffset(start);
    std::cout << " - ";
    printOffset(end - 1);
    std::cout << "  length " << (end - start) << '\n';
}

void printComparisonRanges(const std::vector<std::uint8_t>& first,
                           const std::vector<std::uint8_t>& second)
{
    const std::size_t commonSize = std::min(first.size(), second.size());
    std::cout << "\nComparison ranges (a boundary occurs at each status change):\n";

    std::size_t start = 0;
    while (start < commonSize) {
        const bool same = first[start] == second[start];
        std::size_t end = start + 1;
        while (end < commonSize && (first[end] == second[end]) == same) {
            ++end;
        }

        printRange(same ? "same" : "different", start, end);
        start = end;
    }

    if (commonSize < first.size()) {
        printRange("only A", commonSize, first.size());
    }
    if (commonSize < second.size()) {
        printRange("only B", commonSize, second.size());
    }
    if (first.empty() && second.empty()) {
        std::cout << "(both files are empty)\n";
    }
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: levcompare <first.LEV> <second.LEV>\n";
        return 2;
    }

    const std::filesystem::path firstPath(argv[1]);
    const std::filesystem::path secondPath(argv[2]);

    try {
        const std::vector<std::uint8_t> first = readFile(firstPath);
        const std::vector<std::uint8_t> second = readFile(secondPath);

        std::cout << "File A: " << firstPath.string() << '\n';
        std::cout << "Size A: " << first.size() << " bytes\n";
        std::cout << "File B: " << secondPath.string() << '\n';
        std::cout << "Size B: " << second.size() << " bytes\n";
        std::cout << "Common size: " << std::min(first.size(), second.size())
                  << " bytes\n";

        printFirstDifference(first, second);
        printComparisonRanges(first, second);
    } catch (const std::exception& error) {
        std::cerr << "levcompare: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
