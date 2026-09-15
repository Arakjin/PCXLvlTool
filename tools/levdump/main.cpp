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

constexpr std::size_t kDumpSize = 256;
constexpr std::size_t kBytesPerRow = 16;
constexpr std::size_t kMinimumStringLength = 4;
constexpr std::size_t kMinimumRepeatedRunLength = 17;

bool isPrintableAscii(const std::uint8_t byte)
{
    return byte >= 0x20 && byte <= 0x7e;
}

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

void printHexDump(const std::vector<std::uint8_t>& bytes)
{
    const std::size_t dumpSize = std::min(bytes.size(), kDumpSize);

    std::cout << "\nFirst " << dumpSize << " bytes:\n";
    if (dumpSize == 0) {
        std::cout << "(empty)\n";
        return;
    }

    for (std::size_t offset = 0; offset < dumpSize; offset += kBytesPerRow) {
        const std::size_t rowEnd = std::min(offset + kBytesPerRow, dumpSize);
        printOffset(offset);
        std::cout << "  ";

        for (std::size_t index = offset; index < offset + kBytesPerRow; ++index) {
            if (index < rowEnd) {
                std::cout << std::hex << std::uppercase << std::setw(2)
                          << std::setfill('0')
                          << static_cast<unsigned int>(bytes[index]) << ' ';
            } else {
                std::cout << "   ";
            }
        }

        std::cout << std::dec << std::nouppercase << std::setfill(' ') << " |";
        for (std::size_t index = offset; index < rowEnd; ++index) {
            const std::uint8_t byte = bytes[index];
            std::cout << (isPrintableAscii(byte) ? static_cast<char>(byte) : '.');
        }
        std::cout << "|\n";
    }
}

void printAsciiStrings(const std::vector<std::uint8_t>& bytes)
{
    std::cout << "\nPrintable ASCII strings (length >= "
              << kMinimumStringLength << "):\n";

    bool found = false;
    std::size_t start = 0;

    while (start < bytes.size()) {
        while (start < bytes.size() && !isPrintableAscii(bytes[start])) {
            ++start;
        }

        std::size_t end = start;
        while (end < bytes.size() && isPrintableAscii(bytes[end])) {
            ++end;
        }

        if (end - start >= kMinimumStringLength) {
            found = true;
            printOffset(start);
            std::cout << "  length " << (end - start) << "  \"";
            for (std::size_t index = start; index < end; ++index) {
                std::cout << static_cast<char>(bytes[index]);
            }
            std::cout << "\"\n";
        }

        start = end;
    }

    if (!found) {
        std::cout << "(none)\n";
    }
}

void printRepeatedRuns(const std::vector<std::uint8_t>& bytes)
{
    std::cout << "\nRepeated byte runs (length > 16):\n";

    bool found = false;
    std::size_t start = 0;

    while (start < bytes.size()) {
        std::size_t end = start + 1;
        while (end < bytes.size() && bytes[end] == bytes[start]) {
            ++end;
        }

        const std::size_t length = end - start;
        if (length >= kMinimumRepeatedRunLength) {
            found = true;
            printOffset(start);
            std::cout << "  length " << length << "  value 0x"
                      << std::hex << std::uppercase << std::setw(2)
                      << std::setfill('0')
                      << static_cast<unsigned int>(bytes[start]) << std::dec
                      << std::nouppercase << std::setfill(' ') << '\n';
        }

        start = end;
    }

    if (!found) {
        std::cout << "(none)\n";
    }
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: levdump <file.LEV>\n";
        return 2;
    }

    const std::filesystem::path path(argv[1]);

    try {
        const std::vector<std::uint8_t> bytes = readFile(path);
        std::cout << "File: " << path.string() << '\n';
        std::cout << "File size: " << bytes.size() << " bytes\n";
        printHexDump(bytes);
        printAsciiStrings(bytes);
        printRepeatedRuns(bytes);
    } catch (const std::exception& error) {
        std::cerr << "levdump: " << path.string() << ": " << error.what()
                  << '\n';
        return 1;
    }

    return 0;
}
