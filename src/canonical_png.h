#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool encodeCanonicalRgbaPng(std::uint32_t width, std::uint32_t height,
                            const std::vector<std::uint8_t>& rgba,
                            std::vector<std::uint8_t>& png,
                            std::string& error);

bool decodeCanonicalRgbaPng(const std::vector<std::uint8_t>& png,
                            std::uint32_t& width, std::uint32_t& height,
                            std::vector<std::uint8_t>& rgba,
                            std::string& error);

