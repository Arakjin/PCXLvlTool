#pragma once

constexpr bool isReservedPaletteIndex(const int index)
{
    return (index >= 2 && index <= 15) || index == 31 || index == 38 ||
           index == 47 || (index >= 53 && index <= 55);
}
