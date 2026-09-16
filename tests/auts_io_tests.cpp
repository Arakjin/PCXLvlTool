#include "auts_io.h"
#include "default_palette.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

class TestFile
{
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
    }
    return condition;
}

void setU16(std::vector<std::uint8_t>& bytes, const std::size_t offset,
            const std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void setU32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
            const std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        bytes[offset + static_cast<std::size_t>(shift / 8)] =
            static_cast<std::uint8_t>(value >> shift);
    }
}

std::vector<std::uint8_t> makeAutsBmp()
{
    constexpr std::size_t width = 320;
    constexpr std::size_t height = 400;
    constexpr std::size_t pixelOffset = 14 + 40 + 256 * 4;
    std::vector<std::uint8_t> bytes(pixelOffset + width * height);
    bytes[0] = 'B';
    bytes[1] = 'M';
    setU32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
    setU32(bytes, 10, pixelOffset);
    setU32(bytes, 14, 40);
    setU32(bytes, 18, width);
    setU32(bytes, 22, height);
    setU16(bytes, 26, 1);
    setU16(bytes, 28, 8);
    setU32(bytes, 34, width * height);
    setU32(bytes, 46, 256);
    const auto palette = defaultAutsPalette();
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const std::size_t entry = 54 + index * 4;
        bytes[entry] = palette[index].b;
        bytes[entry + 1] = palette[index].g;
        bytes[entry + 2] = palette[index].r;
    }
    const std::size_t sourceY = height - 1 - 200;
    bytes[pixelOffset + sourceY * width + 160] = 39;
    return bytes;
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
    bool ok = true;
    Level source;
    initializeBlankAutsLevel(source);
    ok &= expect(source.width == 320 && source.height == 400,
                 "blank AUTS dimensions");
    ok &= expect(source.pixels[0] == 7 && source.pixels[320 + 1] == 7 &&
                     source.pixels[2 * 320 + 2] == 0,
                 "blank AUTS two-pixel boundary");
    source.pixels[200 * 320 + 160] = 39;
    source.pixels[0] = 0; // The publisher must restore the game boundary.

    Level blank;
    initializeBlankAutsLevel(blank);
    const TestFile blankLev(std::filesystem::current_path() /
                            "auts-blank-test.lev");
    std::string error;
    ok &= expect(saveAutsLev(blankLev.path(), blank, error),
                 "blank AUTS LEV save: " + error);
    ok &= expect(std::filesystem::file_size(blankLev.path()) == 3590,
                 "blank AUTS LEV must match BMP2LEV encoded size");

    const TestFile lev(std::filesystem::current_path() / "auts-test.lev");
    ok &= expect(saveAutsLev(lev.path(), source, error),
                 "AUTS LEV save: " + error);
    Level loaded;
    ok &= expect(loadAutsLev(lev.path(), loaded, error),
                 "AUTS LEV load: " + error);
    ok &= expect(loaded.width == 320 && loaded.height == 400 &&
                     loaded.pixels[0] == 7 &&
                     loaded.pixels[200 * 320 + 160] == 39,
                 "AUTS LEV round-trip pixels");

    const TestFile bmp(std::filesystem::current_path() / "auts-test.bmp");
    auto bmpBytes = makeAutsBmp();
    writeBytes(bmp.path(), bmpBytes);
    bool paletteMatches = false;
    Level imported;
    ok &= expect(loadAutsBmp(bmp.path(), imported, paletteMatches, error),
                 "AUTS BMP import: " + error);
    ok &= expect(paletteMatches && imported.pixels[200 * 320 + 160] == 39 &&
                     imported.pixels[0] == 7,
                 "AUTS BMP palette, orientation, or boundary");

    bmpBytes[54] ^= 1;
    writeBytes(bmp.path(), bmpBytes);
    ok &= expect(loadAutsBmp(bmp.path(), imported, paletteMatches, error) &&
                     !paletteMatches,
                 "AUTS BMP palette mismatch detection");
    return ok ? 0 : 1;
}
