#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dobong_source {

// Game plane, not latitude/longitude and not screen pixel coordinates.
struct Point { float x; float z; };
enum class GeometryType : std::uint8_t { Unknown, Point, LineString, Polygon };
enum class Quality : std::uint8_t { B1, B2, C, U };
enum class ContextKind : std::uint8_t { Building, Road, Railway, PowerTower, PowerLine, Substation };

struct CourseFeature {
    const char* id;
    const char* kind;
    GeometryType geometry;
    Quality quality;
    std::size_t first;
    std::size_t count;
    float sourceAccuracyMeters;
};

struct ContextFeature {
    const char* id;
    const char* name;
    const char* sourceUrl;
    ContextKind kind;
    GeometryType geometry;
    std::size_t first;
    std::size_t count;
    // -1 is unknown; do not silently replace with a measured height/width.
    float heightMeters;
    float buildingLevels;
    float widthMeters;
    float lanes;
    float osmLayer; // layer is NOT meters above ground.
    bool sourceReportsDisused; // contributor tag; no independent service verification.
    const char* sourceEndDate;
};

inline constexpr double kEarthRadiusMeters = 6378137.0;
inline constexpr double kOriginLongitude = 127.0585674;
inline constexpr double kOriginLatitude = 37.6564809;
inline constexpr double kRadians = 3.14159265358979323846 / 180.0;
inline constexpr float kGameEastOffset = 52.0f;
inline constexpr float kGameNorthOffset = -60.0f;

inline Point EastNorthToGame(double east, double north) {
    return {static_cast<float>(east + kGameEastOffset),
            static_cast<float>(-north + kGameNorthOffset)};
}
inline Point Wgs84ToGame(double longitude, double latitude) {
    const double east = (longitude - kOriginLongitude) * kRadians * kEarthRadiusMeters *
                        std::cos(kOriginLatitude * kRadians);
    const double north = (latitude - kOriginLatitude) * kRadians * kEarthRadiusMeters;
    return EastNorthToGame(east, north);
}

// Pixel helper for the canonical2025-03-22 z19 screenshot, excluding footer.
// Historical2021/2026 screenshots use the same tile grid, NOT equal accuracy.
inline Point CourseImagePixelToGame(double u, double v) {
    constexpr double tiles = 524288.0;
    const double lon = (447184.0 + u / 256.0) / tiles * 360.0 - 180.0;
    const double lat = std::atan(std::sinh(3.14159265358979323846 *
        (1.0 - 2.0 * (202865.0 + v / 256.0) / tiles))) / kRadians;
    return Wgs84ToGame(lon, lat);
}

inline bool SamePoint(Point a, Point b) {
    return a.x == b.x && a.z == b.z;
}
inline bool PointOnSegment(Point p, Point a, Point b, float tolerance = 0.0001f) {
    const double dx = b.x-a.x, dz = b.z-a.z;
    const double lengthSquared = dx*dx+dz*dz;
    if (lengthSquared == 0.0)
        return std::hypot(p.x-a.x, p.z-a.z) <= tolerance;
    const double t = ((p.x-a.x)*dx+(p.z-a.z)*dz) / lengthSquared;
    if (t < 0.0 || t > 1.0) return false;
    return std::hypot(p.x-(a.x+t*dx), p.z-(a.z+t*dz)) <= tolerance;
}

// Boundary-inclusive winding/ray test, accepts open or duplicate-closed rings.
// Float tolerance here is computational only; canonical source accuracy is8.47m.
inline bool ContainsPolygon(const Point* points, std::size_t count, Point p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.z) || count < 3) return false;
    if (SamePoint(points[0], points[count-1])) --count;
    if (count < 3) return false;
    bool inside = false;
    for (std::size_t i=0, j=count-1; i<count; j=i++) {
        const Point a=points[j], b=points[i];
        if (PointOnSegment(p,a,b)) return true;
        if ((a.z>p.z)!=(b.z>p.z)) {
            const double crossing = a.x+(static_cast<double>(p.z)-a.z)*(b.x-a.x)/(b.z-a.z);
            if (p.x<crossing) inside=!inside;
        }
    }
    return inside;
}

} // namespace dobong_source
