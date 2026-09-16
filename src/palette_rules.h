#pragma once

#include "game_profile.h"

constexpr bool isReservedPaletteIndex(const int index)
{
    return (index >= 1 && index <= 15) || index == 31 || index == 38 ||
           index == 47 || (index >= 53 && index <= 55);
}

constexpr bool isReservedPaletteIndex(const GameId game, const int index)
{
    if (game == GameId::Wings) {
        return (index >= 1 && index <= 15) ||
               (index >= 17 && index <= 31) ||
               (index >= 57 && index <= 63);
    }
    return isReservedPaletteIndex(index);
}

constexpr int colorChartNumber(const int paletteIndex)
{
    return paletteIndex;
}

constexpr int paletteIndexFromColorChart(const int colorNumber)
{
    return colorNumber;
}

constexpr bool isReservedColorChartNumber(const int colorNumber)
{
    return isReservedPaletteIndex(paletteIndexFromColorChart(colorNumber));
}

constexpr bool isReservedColorChartNumber(const GameId game,
                                           const int colorNumber)
{
    return isReservedPaletteIndex(game,
                                  paletteIndexFromColorChart(colorNumber));
}
