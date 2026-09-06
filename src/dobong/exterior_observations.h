#pragma once
#include <array>
#include <cstring>

namespace dobong_source {
// Qualitative aerial observations only. RGB values are illustrative, not
// calibrated samples. These override roof color, never footprint or height.
// See docs/dobong/EXTERIOR.md for source dates, uncertainty and excluded roofs.
struct RoofObservation {
    const char* id;
    unsigned char r, g, b;
};
inline constexpr std::array<RoofObservation, 5> kRoofObservations{{
    {"way/380676074", 68, 109, 99},   // Main hall: broad green/teal roof.
    {"way/887784323", 80, 124, 83},   // Southwest large green roof.
    {"way/911080722", 94, 92, 85},    // Long narrow grey/brown roof.
    {"way/911080726", 76, 86, 94},    // North grey/blue roof, not its green neighbour.
    {"way/1226934249", 70, 121, 85},  // Northeast green rectangle, NOT the SE shelter.
}};
inline const RoofObservation* FindRoofObservation(const char* id) {
    if (!id) return nullptr;
    for (const auto& roof : kRoofObservations)
        if (std::strcmp(roof.id, id) == 0) return &roof;
    return nullptr;
}
} // namespace dobong_source
