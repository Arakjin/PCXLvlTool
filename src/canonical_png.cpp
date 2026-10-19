#include "canonical_png.h"

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::array<std::uint8_t, 8> kSignature{
    137, 80, 78, 71, 13, 10, 26, 10};

void appendBe32(std::vector<std::uint8_t>& output, const std::uint32_t value)
{
    output.push_back(static_cast<std::uint8_t>(value >> 24));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t readBe32(const std::vector<std::uint8_t>& bytes,
                       const std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("truncated canonical PNG");
    }
    return static_cast<std::uint32_t>(bytes[offset]) << 24 |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 8 |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::uint32_t crc32(const std::uint8_t* data, const std::size_t size)
{
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xedb88320U & (0U - static_cast<std::uint32_t>(crc & 1U)));
        }
    }
    return crc ^ 0xffffffffU;
}

std::uint32_t adler32(const std::vector<std::uint8_t>& bytes)
{
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t byte : bytes) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    return b << 16 | a;
}

void appendChunk(std::vector<std::uint8_t>& output,
                 const std::string_view type,
                 const std::vector<std::uint8_t>& data)
{
    appendBe32(output, static_cast<std::uint32_t>(data.size()));
    const std::size_t crcStart = output.size();
    output.insert(output.end(), type.begin(), type.end());
    output.insert(output.end(), data.begin(), data.end());
    appendBe32(output, crc32(output.data() + crcStart,
                             output.size() - crcStart));
}

void validateDimensions(const std::uint32_t width, const std::uint32_t height,
                        const std::size_t rgbaSize)
{
    if (width == 0 || height == 0 || width > 8192 || height > 8192 ||
        static_cast<std::uint64_t>(width) * height > 33554432ULL ||
        static_cast<std::uint64_t>(width) * height * 4ULL != rgbaSize ||
        rgbaSize > 134217728ULL) {
        throw std::runtime_error("background RGBA dimensions are invalid");
    }
}

} // namespace

bool encodeCanonicalRgbaPng(const std::uint32_t width,
                            const std::uint32_t height,
                            const std::vector<std::uint8_t>& rgba,
                            std::vector<std::uint8_t>& png,
                            std::string& error)
{
    error.clear();
    try {
        validateDimensions(width, height, rgba.size());
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        std::vector<std::uint8_t> filtered;
        filtered.reserve((rowBytes + 1) * height);
        for (std::uint32_t y = 0; y < height; ++y) {
            filtered.push_back(0);
            const auto begin = rgba.begin() +
                static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) *
                                            rowBytes);
            filtered.insert(filtered.end(), begin, begin + rowBytes);
        }

        std::vector<std::uint8_t> zlib{0x78, 0x01};
        std::size_t offset = 0;
        do {
            const std::size_t count =
                std::min<std::size_t>(65535, filtered.size() - offset);
            const bool finalBlock = offset + count == filtered.size();
            zlib.push_back(finalBlock ? 1 : 0);
            zlib.push_back(static_cast<std::uint8_t>(count));
            zlib.push_back(static_cast<std::uint8_t>(count >> 8));
            const std::uint16_t inverse =
                static_cast<std::uint16_t>(~static_cast<std::uint16_t>(count));
            zlib.push_back(static_cast<std::uint8_t>(inverse));
            zlib.push_back(static_cast<std::uint8_t>(inverse >> 8));
            zlib.insert(zlib.end(), filtered.begin() +
                                      static_cast<std::ptrdiff_t>(offset),
                        filtered.begin() +
                            static_cast<std::ptrdiff_t>(offset + count));
            offset += count;
        } while (offset < filtered.size());
        appendBe32(zlib, adler32(filtered));

        png.assign(kSignature.begin(), kSignature.end());
        std::vector<std::uint8_t> ihdr;
        appendBe32(ihdr, width);
        appendBe32(ihdr, height);
        ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
        appendChunk(png, "IHDR", ihdr);
        appendChunk(png, "sRGB", std::vector<std::uint8_t>{0});
        appendChunk(png, "IDAT", zlib);
        appendChunk(png, "IEND", {});
        if (png.size() > 136ULL * 1024ULL * 1024ULL) {
            throw std::runtime_error("canonical background PNG is too large");
        }
        return true;
    } catch (const std::exception& exception) {
        png.clear();
        error = exception.what();
        return false;
    }
}

bool decodeCanonicalRgbaPng(const std::vector<std::uint8_t>& png,
                            std::uint32_t& width, std::uint32_t& height,
                            std::vector<std::uint8_t>& rgba,
                            std::string& error)
{
    error.clear();
    try {
        if (png.size() < kSignature.size() ||
            !std::equal(kSignature.begin(), kSignature.end(), png.begin())) {
            throw std::runtime_error("invalid canonical PNG signature");
        }
        std::size_t offset = kSignature.size();
        std::vector<std::uint8_t> zlib;
        int expectedChunk = 0;
        while (offset < png.size()) {
            const std::uint32_t size = readBe32(png, offset);
            offset += 4;
            if (offset > png.size() || png.size() - offset < 8ULL + size) {
                throw std::runtime_error("truncated canonical PNG chunk");
            }
            const std::string type(reinterpret_cast<const char*>(png.data() +
                                                                    offset),
                                   4);
            const std::array<const char*, 4> order{"IHDR", "sRGB", "IDAT",
                                                   "IEND"};
            if (expectedChunk >= 4 || type != order[expectedChunk++]) {
                throw std::runtime_error("non-canonical PNG chunk order");
            }
            const std::size_t crcStart = offset;
            offset += 4;
            const std::uint32_t expectedCrc = readBe32(png, offset + size);
            if (crc32(png.data() + crcStart, size + 4) != expectedCrc) {
                throw std::runtime_error("canonical PNG CRC mismatch");
            }
            if (type == "IHDR") {
                if (size != 13 || png[offset + 8] != 8 ||
                    png[offset + 9] != 6 || png[offset + 10] != 0 ||
                    png[offset + 11] != 0 || png[offset + 12] != 0) {
                    throw std::runtime_error("unsupported canonical PNG header");
                }
                width = readBe32(png, offset);
                height = readBe32(png, offset + 4);
            } else if (type == "sRGB") {
                if (size != 1 || png[offset] != 0) {
                    throw std::runtime_error("invalid canonical PNG sRGB chunk");
                }
            } else if (type == "IDAT") {
                zlib.assign(png.begin() + static_cast<std::ptrdiff_t>(offset),
                            png.begin() +
                                static_cast<std::ptrdiff_t>(offset + size));
            } else if (size != 0) {
                throw std::runtime_error("invalid canonical PNG IEND chunk");
            }
            offset += size + 4;
        }
        if (expectedChunk != 4 || offset != png.size() || zlib.size() < 11 ||
            zlib[0] != 0x78 || zlib[1] != 0x01) {
            throw std::runtime_error("incomplete canonical PNG");
        }
        std::vector<std::uint8_t> filtered;
        std::size_t input = 2;
        bool finalBlock = false;
        while (!finalBlock) {
            if (input + 5 > zlib.size() - 4) {
                throw std::runtime_error("truncated canonical PNG DEFLATE block");
            }
            const std::uint8_t header = zlib[input++];
            finalBlock = (header & 1U) != 0;
            if ((header & 0xfeU) != 0) {
                throw std::runtime_error("canonical PNG requires stored blocks");
            }
            const std::uint16_t length = static_cast<std::uint16_t>(
                zlib[input] | static_cast<std::uint16_t>(zlib[input + 1]) << 8);
            const std::uint16_t inverse = static_cast<std::uint16_t>(
                zlib[input + 2] |
                static_cast<std::uint16_t>(zlib[input + 3]) << 8);
            input += 4;
            if (static_cast<std::uint16_t>(~length) != inverse ||
                input + length > zlib.size() - 4) {
                throw std::runtime_error("invalid canonical PNG stored block");
            }
            if (!finalBlock && length != 65535) {
                throw std::runtime_error(
                    "non-final canonical PNG block has the wrong size");
            }
            filtered.insert(filtered.end(),
                            zlib.begin() + static_cast<std::ptrdiff_t>(input),
                            zlib.begin() +
                                static_cast<std::ptrdiff_t>(input + length));
            input += length;
        }
        if (input != zlib.size() - 4 ||
            readBe32(zlib, input) != adler32(filtered)) {
            throw std::runtime_error("canonical PNG Adler-32 mismatch");
        }
        const std::uint64_t rowBytes = static_cast<std::uint64_t>(width) * 4;
        if (static_cast<std::uint64_t>(height) * (rowBytes + 1) !=
            filtered.size()) {
            throw std::runtime_error("canonical PNG pixel size mismatch");
        }
        rgba.clear();
        rgba.reserve(static_cast<std::size_t>(rowBytes * height));
        std::size_t rowOffset = 0;
        for (std::uint32_t y = 0; y < height; ++y) {
            if (filtered[rowOffset++] != 0) {
                throw std::runtime_error("canonical PNG requires filter None");
            }
            rgba.insert(rgba.end(),
                        filtered.begin() +
                            static_cast<std::ptrdiff_t>(rowOffset),
                        filtered.begin() + static_cast<std::ptrdiff_t>(
                                               rowOffset + rowBytes));
            rowOffset += static_cast<std::size_t>(rowBytes);
        }
        validateDimensions(width, height, rgba.size());
        return true;
    } catch (const std::exception& exception) {
        width = 0;
        height = 0;
        rgba.clear();
        error = exception.what();
        return false;
    }
}
