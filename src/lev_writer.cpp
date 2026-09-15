#include "lev_writer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <stdexcept>

namespace {

constexpr std::size_t kMaximumNameLength = 20;
constexpr std::size_t kNameOffset = 2;
constexpr std::size_t kHeaderTailOffset = 25;

constexpr std::array<std::uint8_t, 103> kClassicHeaderTail{{
    0x08, 0x00, 0x09, 0x05, 0x0d, 0x05, 0x12,
    0x18, 0x07, 0x0b, 0x02, 0x12, 0x0e, 0x01, 0x04, 0x15, 0x11, 0x13, 0x15,
    0x18, 0x05, 0x0b, 0x18, 0x15, 0x18, 0x15, 0x0b, 0x0f, 0x11, 0x0f, 0x0e,
    0x12, 0x02, 0x0a, 0x03, 0x11, 0x0c, 0x0c, 0x0e, 0x08, 0x16, 0x06, 0x04,
    0x0a, 0x12, 0x04, 0x0d, 0x10, 0x10, 0x00, 0x14, 0x06, 0x0b, 0x0a, 0x09,
    0x0f, 0x0f, 0x07, 0x04, 0x14, 0x07, 0x08, 0x0a, 0x06, 0x0a, 0x01, 0x0c,
    0x19, 0x00, 0x0a, 0x01, 0x01, 0x06, 0x19, 0x09, 0x0a, 0x05, 0x05, 0x05,
    0x06, 0x13, 0x17, 0x17, 0x13, 0x0b, 0x0d, 0x00, 0x03, 0x0b, 0x0c, 0x0f,
    0x06, 0x02, 0x12, 0x10, 0x17, 0x16, 0x01, 0x0e, 0x0d, 0x10, 0x14, 0x06,
}};

bool isPrintableAscii(const char character)
{
    const auto value = static_cast<unsigned char>(character);
    return value >= 0x20 && value <= 0x7e;
}

std::array<std::uint8_t, 128> makeHeader(const std::string& name)
{
    if (name.size() > kMaximumNameLength) {
        throw std::runtime_error("level name is longer than 20 bytes");
    }
    if (!std::all_of(name.begin(), name.end(), isPrintableAscii)) {
        throw std::runtime_error("level name must contain printable ASCII only");
    }

    std::array<std::uint8_t, 128> header{};
    header[0] = 0x76;
    header[1] = 0x07;
    std::fill(header.begin() + kNameOffset,
              header.begin() + kNameOffset + kMaximumNameLength + 1, 0x20);
    std::transform(name.begin(), name.end(), header.begin() + kNameOffset,
                   [](const char character) {
                       const char uppercase =
                           character >= 'a' && character <= 'z'
                               ? static_cast<char>(character - 'a' + 'A')
                               : character;
                       return static_cast<std::uint8_t>(uppercase);
                   });
    header[kNameOffset + name.size()] = 0;
    std::copy(kClassicHeaderTail.begin(), kClassicHeaderTail.end(),
              header.begin() + kHeaderTailOffset);
    return header;
}

void writeRun(std::ofstream& output, const std::uint8_t value,
              const std::size_t length)
{
    if (length > 1 || value >= 0xc0) {
        output.put(static_cast<char>(0xc0 | length));
    }
    output.put(static_cast<char>(value));
}

} // namespace

bool saveLev(const std::filesystem::path& path, const Level& level,
             std::string& error)
{
    error.clear();

    try {
        const std::array<std::uint8_t, 128> header = makeHeader(level.name);
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create output file");
        }

        output.write(reinterpret_cast<const char*>(header.data()),
                     static_cast<std::streamsize>(header.size()));
        for (std::size_t y = 0; y < Level::Height; ++y) {
            const std::size_t rowStart = y * Level::Width;
            std::size_t x = 0;
            while (x < Level::Width) {
                const std::uint8_t value = level.pixels[rowStart + x];
                std::size_t length = 1;
                while (x + length < Level::Width && length < 63 &&
                       level.pixels[rowStart + x + length] == value) {
                    ++length;
                }
                writeRun(output, value, length);
                x += length;
            }
        }

        output.put(static_cast<char>(0x0c));
        for (const RGB& color : level.palette) {
            output.put(static_cast<char>(color.r));
            output.put(static_cast<char>(color.g));
            output.put(static_cast<char>(color.b));
        }

        output.close();
        if (!output) {
            throw std::runtime_error("could not write the complete LEV file");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
