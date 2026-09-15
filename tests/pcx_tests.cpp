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
    std::cout << "All PCX writer tests passed\n";
    return 0;
}
