#pragma once

constexpr bool isReservedPaletteIndex(const int index)
{
    return (index >= 1 && index <= 15) || index == 31 || index == 38 ||
           index == 47 || (index >= 53 && index <= 55);
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
