#include "pcx_reader.h"
#include "pcx_writer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

class TestFile {
public:
    explicit TestFile(std::filesystem::path path) : path_(std::move(path)) {}

    ~TestFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

bool expect(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

std::uint16_t littleEndian16(const std::vector<std::uint8_t>& bytes,
                             const std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1] << 8);
}

void putLittleEndian16(std::vector<std::uint8_t>& bytes,
                       const std::size_t offset, const std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void writeBytes(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

} // namespace

int main()
{
    const TestFile file(std::filesystem::current_path() / "pcx-writer-test.pcx");
    auto level = std::make_unique<Level>();
    level->pixels.fill(0);
    for (std::size_t index = 0; index < level->palette.size(); ++index) {
        level->palette[index] = RGB{
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(255 - index),
            static_cast<std::uint8_t>(index / 2),
        };
    }

    std::string error;
    bool ok = expect(savePcx(file.path(), *level, error),
                     "valid PCX could not be written: " + error);
    const std::vector<std::uint8_t> bytes = readBytes(file.path());

    constexpr std::size_t encodedRowSize = 22;
    constexpr std::size_t paletteMarkerOffset =
        128 + Level::Height * encodedRowSize;
    constexpr std::size_t expectedSize = paletteMarkerOffset + 1 + 256 * 3;
    ok &= expect(bytes.size() == expectedSize, "unexpected PCX file size");
    if (bytes.size() == expectedSize) {
        ok &= expect(bytes[0] == 0x0a && bytes[1] == 5 && bytes[2] == 1 &&
                         bytes[3] == 8,
                     "invalid PCX identity fields");
        ok &= expect(littleEndian16(bytes, 8) == 639 &&
                         littleEndian16(bytes, 10) == 799,
                     "invalid PCX bounds");
        ok &= expect(bytes[65] == 1 && littleEndian16(bytes, 66) == 640,
                     "invalid PCX plane layout");

        std::size_t offset = 128;
        for (std::size_t y = 0; y < Level::Height; ++y) {
            for (std::size_t run = 0; run < 10; ++run) {
                ok &= expect(bytes[offset++] == 0xff && bytes[offset++] == 0,
                             "unexpected 63-byte RLE run");
            }
            ok &= expect(bytes[offset++] == 0xca && bytes[offset++] == 0,
                         "unexpected final scanline RLE run");
        }
        ok &= expect(offset == paletteMarkerOffset && bytes[offset] == 0x0c,
                     "missing PCX palette marker");

        const std::size_t paletteOffset = paletteMarkerOffset + 1;
        ok &= expect(bytes[paletteOffset + 42 * 3] == 42 &&
                         bytes[paletteOffset + 42 * 3 + 1] == 213 &&
                         bytes[paletteOffset + 42 * 3 + 2] == 21,
                     "palette entry was not preserved");
    }

    Level loaded;
    error.clear();
    ok &= expect(loadPcx(file.path(), loaded, error),
                 "valid PCX could not be loaded: " + error);
    ok &= expect(loaded.width == Level::Width &&
                     loaded.height == Level::Height &&
                     loaded.pixels == level->pixels &&
                     loaded.palette == level->palette,
                 "PCX reader/writer round-trip did not preserve indexed data");

    const TestFile variable(std::filesystem::current_path() /
                            "pcx-variable-test.pcx");
    std::vector<std::uint8_t> variableBytes(128);
    variableBytes[0] = 0x0a;
    variableBytes[1] = 5;
    variableBytes[2] = 1;
    variableBytes[3] = 8;
    putLittleEndian16(variableBytes, 8, 2);  // width = 3
    putLittleEndian16(variableBytes, 10, 1); // height = 2
    variableBytes[65] = 1;
    putLittleEndian16(variableBytes, 66, 4); // one padding byte per row
    variableBytes.insert(variableBytes.end(), {1, 2, 3, 99, 4, 5, 6, 88});
    variableBytes.push_back(0x0c);
    variableBytes.resize(variableBytes.size() + 256 * 3);
    writeBytes(variable.path(), variableBytes);
    error.clear();
    ok &= expect(loadPcx(variable.path(), loaded, error),
                 "variable-size PCX could not be loaded: " + error);
    ok &= expect(loaded.width == 3 && loaded.height == 2 &&
                     loaded.pixels.size() == 6 && loaded.pixels[0] == 1 &&
                     loaded.pixels[1] == 2 && loaded.pixels[2] == 3 &&
                     loaded.pixels[3] == 4 && loaded.pixels[4] == 5 &&
                     loaded.pixels[5] == 6,
                 "PCX reader did not remove scanline padding");

    const std::filesystem::path invalidPath =
        std::filesystem::current_path() / "pcx-writer-missing-directory" /
        "output.pcx";
    error.clear();
    ok &= expect(!savePcx(invalidPath, *level, error),
                 "writer accepted a missing output directory");
    ok &= expect(!error.empty(), "writer did not report its output error");

    if (!ok) {
        return 1;
    }
    std::cout << "All PCX tests passed\n";
    return 0;
}
