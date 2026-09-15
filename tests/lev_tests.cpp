#include "lev_reader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kHeaderSize = 128;
constexpr std::size_t kPaletteSize = 256 * 3;

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

std::vector<std::uint8_t> makeValidLev()
{
    std::vector<std::uint8_t> bytes(kHeaderSize, 0);
    bytes[0] = 0x76;
    bytes[1] = 0x07;
    const std::string name = "TEST LEVEL";
    std::copy(name.begin(), name.end(), bytes.begin() + 2);

    for (std::size_t y = 0; y < Level::Height; ++y) {
        std::size_t rowRemaining = Level::Width;
        while (rowRemaining != 0) {
            const std::size_t count = std::min<std::size_t>(63, rowRemaining);
            bytes.push_back(static_cast<std::uint8_t>(0xc0 | count));
            bytes.push_back(57);
            rowRemaining -= count;
        }
    }

    bytes.push_back(0x0c);
    for (std::size_t index = 0; index < 256; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(index));
        bytes.push_back(static_cast<std::uint8_t>(255 - index));
        bytes.push_back(static_cast<std::uint8_t>(index / 2));
    }
    return bytes;
}

bool writeBytes(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

bool expect(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool testValidFile(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = makeValidLev();
    if (!writeBytes(path, bytes)) {
        return expect(false, "could not write valid test file");
    }

    auto level = std::make_unique<Level>();
    std::string error;
    bool ok = expect(loadLev(path, *level, error),
                     "valid file was rejected: " + error);
    ok &= expect(level->name == "TEST LEVEL", "level name was not decoded");
    ok &= expect(level->pixels.front() == 57 && level->pixels.back() == 57,
                 "pixel RLE was not decoded");
    ok &= expect(level->palette[42] == RGB{42, 213, 21},
                 "palette was not decoded");
    return ok;
}

bool expectRejected(const std::filesystem::path& path,
                    const std::vector<std::uint8_t>& bytes,
                    const std::string& expectedError)
{
    if (!writeBytes(path, bytes)) {
        return expect(false, "could not write invalid test file");
    }

    auto level = std::make_unique<Level>();
    std::string error;
    const bool loaded = loadLev(path, *level, error);
    return expect(!loaded, "invalid file was accepted") &&
           expect(error.find(expectedError) != std::string::npos,
                  "unexpected error: " + error);
}

template <typename ExpectedPixel>
bool verifyOracleFile(const std::filesystem::path& directory,
                      const std::string& filename,
                      ExpectedPixel expectedPixel,
                      const bool changedPalette = false)
{
    auto level = std::make_unique<Level>();
    std::string error;
    if (!expect(loadLev(directory / filename, *level, error),
                filename + " was rejected: " + error)) {
        return false;
    }

    for (std::size_t y = 0; y < Level::Height; ++y) {
        for (std::size_t x = 0; x < Level::Width; ++x) {
            const std::uint8_t expected = expectedPixel(x, y);
            if (level->pixels[y * Level::Width + x] != expected) {
                return expect(false, filename + " has an unexpected pixel at " +
                                         std::to_string(x) + "," +
                                         std::to_string(y));
            }
        }
    }

    for (std::size_t index = 0; index < level->palette.size(); ++index) {
        RGB expected{
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(index),
        };
        if (changedPalette && index == 57) {
            expected = RGB{1, 2, 3};
        }
        if (!(level->palette[index] == expected)) {
            return expect(false, filename + " has an unexpected palette entry " +
                                     std::to_string(index));
        }
    }
    return true;
}

bool testOracleCorpus(const std::filesystem::path& directory)
{
    bool ok = true;
    ok &= verifyOracleFile(directory, "T00.LEV",
                           [](std::size_t, std::size_t) { return 0; });
    ok &= verifyOracleFile(directory, "T01.LEV",
                           [](std::size_t, std::size_t) { return 1; });
    ok &= verifyOracleFile(directory, "T57.LEV",
                           [](std::size_t, std::size_t) { return 57; });

    struct PixelFixture {
        const char* name;
        std::size_t x;
        std::size_t y;
    };
    constexpr std::array<PixelFixture, 6> pixelFixtures{{
        {"P000.LEV", 0, 0},
        {"P100.LEV", 1, 0},
        {"P6390.LEV", 639, 0},
        {"P001.LEV", 0, 1},
        {"P0799.LEV", 0, 799},
        {"P639799.LEV", 639, 799},
    }};
    for (const PixelFixture& fixture : pixelFixtures) {
        ok &= verifyOracleFile(directory, fixture.name,
                               [fixture](const std::size_t x,
                                         const std::size_t y) {
                                   return static_cast<std::uint8_t>(
                                       x == fixture.x && y == fixture.y);
                               });
    }

    ok &= verifyOracleFile(directory, "VSTRIPE.LEV",
                           [](const std::size_t x, std::size_t) {
                               return static_cast<std::uint8_t>((x / 8) % 2);
                           });
    ok &= verifyOracleFile(directory, "HSTRIPE.LEV",
                           [](std::size_t, const std::size_t y) {
                               return static_cast<std::uint8_t>((y / 8) % 2);
                           });
    ok &= verifyOracleFile(directory, "CHECKER.LEV",
                           [](const std::size_t x, const std::size_t y) {
                               return static_cast<std::uint8_t>(
                                   ((x / 8) + (y / 8)) % 2);
                           });
    ok &= verifyOracleFile(directory, "LONGRUN.LEV",
                           [](const std::size_t x, std::size_t) {
                               return static_cast<std::uint8_t>(
                                   x < 320 ? 57 : 221);
                           });

    std::uint32_t randomState = 0x564c4556;
    ok &= verifyOracleFile(directory, "RANDOM.LEV",
                           [&randomState](std::size_t, std::size_t) {
                               randomState = randomState * 1664525u + 1013904223u;
                               return static_cast<std::uint8_t>(randomState >> 24);
                           });
    ok &= verifyOracleFile(directory, "T00PAL.LEV",
                           [](std::size_t, std::size_t) { return 0; }, true);
    return ok;
}

} // namespace

int main(int argc, char* argv[])
{
    const TestFile file(std::filesystem::current_path() / "lev-reader-test.lev");
    bool ok = testValidFile(file.path());

    std::vector<std::uint8_t> bytes = makeValidLev();
    bytes[0] = 0;
    ok &= expectRejected(file.path(), bytes, "signature");

    bytes = makeValidLev();
    bytes[bytes.size() - kPaletteSize - 1] = 0;
    ok &= expectRejected(file.path(), bytes, "palette marker");

    bytes.assign(kHeaderSize + 1 + kPaletteSize, 0);
    bytes[0] = 0x76;
    bytes[1] = 0x07;
    bytes[kHeaderSize] = 0x0c;
    ok &= expectRejected(file.path(), bytes, "decoded to 0 pixels");

    bytes = makeValidLev();
    const std::size_t marker = bytes.size() - kPaletteSize - 1;
    bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(marker), 1);
    ok &= expectRejected(file.path(), bytes, "unexpected data");

    bytes.assign(kHeaderSize, 0);
    bytes[0] = 0x76;
    bytes[1] = 0x07;
    bytes.push_back(0xc0);
    bytes.push_back(0x0c);
    bytes.resize(bytes.size() + kPaletteSize, 0);
    ok &= expectRejected(file.path(), bytes, "zero-length");

    bytes.assign(kHeaderSize, 0);
    bytes[0] = 0x76;
    bytes[1] = 0x07;
    bytes.push_back(0xc1);
    bytes.push_back(0x0c);
    bytes.resize(bytes.size() + kPaletteSize, 0);
    ok &= expectRejected(file.path(), bytes, "has no value byte");

    bytes = makeValidLev();
    bytes[kHeaderSize + 20] = 0xcb;
    ok &= expectRejected(file.path(), bytes, "scanline");

    if (argc == 2) {
        ok &= testOracleCorpus(argv[1]);
    } else if (argc != 1) {
        std::cerr << "Usage: lev_tests [oracle-directory]\n";
        return 2;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "All LEV reader tests passed\n";
    return 0;
}
