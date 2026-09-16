#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class GameId : std::uint8_t {
    VWing,
    Wings,
    Auts,
};

enum class GameFeature : std::uint32_t {
    None = 0,
    VariableLevelSize = 1U << 0,
    ParallaxBackground = 1U << 1,
    Stars = 1U << 2,
    Weather = 1U << 3,
    Civilians = 1U << 4,
};

constexpr GameFeature operator|(const GameFeature left,
                                const GameFeature right)
{
    return static_cast<GameFeature>(static_cast<std::uint32_t>(left) |
                                    static_cast<std::uint32_t>(right));
}

constexpr bool hasFeature(const GameFeature features,
                          const GameFeature feature)
{
    return (static_cast<std::uint32_t>(features) &
            static_cast<std::uint32_t>(feature)) != 0;
}

struct GameProfile {
    GameId id;
    std::string_view key;
    std::string_view displayName;
    int minimumWidth;
    int minimumHeight;
    int maximumWidth;
    int maximumHeight;
    int defaultWidth;
    int defaultHeight;
    GameFeature features;
};

enum class BackgroundMode : std::uint8_t {
    None,
    Parallax,
};

struct LevelCreationSettings {
    GameId game = GameId::VWing;
    std::string name = "UNTITLED";
    int width = 640;
    int height = 800;
    BackgroundMode backgroundMode = BackgroundMode::None;
    bool stars = false;
    int rainProbability = 8;
    int snowProbability = 4;
    int bombingProbability = 2;
    int civilians = 40;
    int armedCiviliansProbability = 50;
};

const GameProfile& gameProfile(GameId id);
const GameProfile* gameProfileByKey(std::string_view key);
const std::vector<GameProfile>& availableGameProfiles();

// Wings' original MAKELEV documentation uses integer division here.
constexpr std::pair<int, int> wingsParallaxSize(const int levelWidth,
                                                const int levelHeight)
{
    return {levelWidth / 2 + 78, levelHeight / 2 + 45};
}
