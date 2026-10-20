#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct OverThrustBackground {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
    std::array<std::uint8_t, 32> contentHash{};
    std::string sourceProvenance;

    bool empty() const { return rgba.empty(); }
};

struct OverThrustTransform {
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
    int rotationMillidegrees = 0;
};

struct OverThrustPoint {
    int x = 0;
    int y = 0;

    bool operator==(const OverThrustPoint& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct OverThrustAccess {
    bool everyone = true;
    std::vector<int> playerSlots;
};

struct OverThrustTaggedProperty {
    std::string name;
    std::string type;
    std::string value;
};

struct OverThrustObject {
    std::string id;
    std::string type;
    OverThrustTransform transform;
    std::string appearanceId;

    // Sliding door fields.
    int openOffsetX = 0;
    int openOffsetY = 0;
    double speedPixelsPerSecond = 4.0;
    std::string initialState = "closed";
    std::string activationMode = "proximity";
    double activationRadiusPixels = 4.0;
    std::string activationChannel;
    std::string materialId;

    // Teleporter and service-volume fields.
    double radiusPixels = 4.0;
    std::string targetObjectId;
    std::string velocityMode = "preserve";
    std::string serviceProfileId;
    std::vector<std::string> services;
    OverThrustAccess access;

    // Team-base and capture-flag fields.
    int teamSlot = 1;
    std::string baseProfileId;
    // Retained for backward-compatible PXLP reads; current team bases use a
    // separate capture_flag object instead of an embedded objective anchor.
    int objectiveAnchorX = 0;
    int objectiveAnchorY = 0;
    // Absolute world-pixel boundary. Its interior is the authoritative team
    // base area.
    std::vector<OverThrustPoint> areaPolygon;

    // Retained when a newer PXLP producer adds fields unknown to this build.
    // Publishing rejects properties unknown to the active profile revision.
    std::vector<OverThrustTaggedProperty> unknownProperties;
};

struct OverThrustChannel {
    std::string id;
    bool initiallyActive = false;
};

struct OverThrustTeamBinding {
    int teamSlot = 1;
    std::string baseObjectId;
};

struct OverThrustTeamModeSetting {
    std::string gameModeId;
    std::vector<OverThrustTeamBinding> teams;
};

struct OverThrustProjectData {
    std::string levelId = "Untitled";
    std::string displayName = "Untitled";
    std::string author;
    std::string description;
    bool backgroundParallax = false;
    double backgroundScrollX = 1.0;
    double backgroundScrollY = 1.0;
    int captureTheFlagPlayers = 2;
    std::vector<std::string> allowedGameModes{"core:sandbox"};
    std::vector<OverThrustTeamModeSetting> teamModeSettings;
    OverThrustBackground background;
    std::vector<OverThrustChannel> channels;
    std::vector<OverThrustObject> objects;
};

