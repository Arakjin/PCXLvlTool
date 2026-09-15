#pragma once

constexpr bool isReservedPaletteIndex(const int index)
{
    return (index >= 1 && index <= 14) || index == 30 || index == 37 ||
           index == 46 || (index >= 52 && index <= 54);
}

constexpr int colorChartNumber(const int paletteIndex)
{
    return paletteIndex + 1;
}

constexpr int paletteIndexFromColorChart(const int colorNumber)
{
    return colorNumber - 1;
}

constexpr bool isReservedColorChartNumber(const int colorNumber)
{
    return isReservedPaletteIndex(paletteIndexFromColorChart(colorNumber));
}
