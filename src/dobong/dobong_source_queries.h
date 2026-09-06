#pragma once
#include "dobong_observed_data.h"
#include "dobong_context_data.h"
#include "dobong_dem_data.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace dobong_source {

inline const CourseFeature* FindCourseFeature(const char* id) {
    for (const auto& f : kCourseFeatures) if (std::strcmp(f.id,id)==0) return &f;
    return nullptr;
}
inline bool ContainsFeature(const CourseFeature& f, Point p) {
    return f.geometry==GeometryType::Polygon &&
           ContainsPolygon(kCourseVertices.data()+f.first,f.count,p);
}
inline bool InFeature(const char* id, Point p) {
    const auto* f=FindCourseFeature(id);
    return f && ContainsFeature(*f,p);
}
inline bool InOuterEnvelope(Point p) {
    return InFeature("ordinary-course-outer-curb-observed",p);
}
inline bool InObservedIsland(Point p) {
    return InFeature("northwest-island-observed",p) ||
           InFeature("northeast-island-observed",p);
}
inline bool InObservedParkingAccess(Point p) {
    return InFeature("parking-t-access-north-west-entry-observed",p) ||
           InFeature("parking-t-access-middle-north-east-entry-observed",p) ||
           InFeature("parking-t-access-middle-south-west-entry-observed",p) ||
           InFeature("parking-t-access-south-east-entry-observed",p);
}
// Compatibility name; the old3 inferred gaps are REMOVED from canonical data.
inline bool InCandidateParkingGap(Point p) { return InObservedParkingAccess(p); }
inline bool InObservedExcludedPavement(Point p) {
    return InFeature("northeast-expansion-hatching-observed",p) ||
           InFeature("southwest-expansion-hatching-observed",p);
}

enum class SurfaceClass {
    Outside,
    HistoricalPavement,
    NonDrivableIsland,
    UnverifiedParkingComplex,
    ObservedParkingAccess,
    CandidateParkingGap = ObservedParkingAccess, // backward-compatible enum name
    CandidateVehicleStaging
};

// Direct2025 observations only. Not a test scoring boundary or vehicle-safe claim.
// An untraced lawn inside the parking-complex envelope must never become pavement.
inline SurfaceClass ClassifySurface(Point p) {
    if (!InOuterEnvelope(p)) return SurfaceClass::Outside;
    if (InObservedIsland(p) || InObservedExcludedPavement(p))
        return SurfaceClass::NonDrivableIsland;
    if (InObservedParkingAccess(p)) return SurfaceClass::ObservedParkingAccess;
    if (InFeature("parking-complex-envelope-observed",p))
        return SurfaceClass::UnverifiedParkingComplex;
    if (InFeature("vehicle-staging-apron-observed",p))
        return SurfaceClass::CandidateVehicleStaging;
    return SurfaceClass::HistoricalPavement;
}

// Conservative preview helper: parking/staging participation requires opt-in.
// Parking complex grass/unknown portions remain rejected even with opt-in.
inline bool IsObservedRoadSurface(Point p, bool allowCandidateParkingGaps=false,
                                  bool allowCandidateStaging=false) {
    const auto c=ClassifySurface(p);
    return c==SurfaceClass::HistoricalPavement ||
           (allowCandidateParkingGaps && c==SurfaceClass::CandidateParkingGap) ||
           (allowCandidateStaging && c==SurfaceClass::CandidateVehicleStaging);
}
inline constexpr bool kHasVerifiedStartLine = false;
inline constexpr bool kHasVerifiedFinishLine = false;
inline constexpr bool kHasVerifiedSlopeProfile = false;
inline constexpr bool kHasVerifiedRouteSequence = false;
inline constexpr bool kHasHistoricalOfficial2023RouteTopology = true;
inline constexpr bool kGeometryIsSurveyGrade = false;

// NaN, infinity or out-of-coverage coordinates return false and do not modify out.
// Game grid is 125m sampled background relief, NOT local wheel/contact height.
inline bool TrySampleTerrain(Point p, float& out) {
    if (!std::isfinite(p.x) || !std::isfinite(p.z)) return false;
    const double east=static_cast<double>(p.x)-kGameEastOffset;
    const double north=-static_cast<double>(p.z)+kGameNorthOffset;
    const double u=(east-kDemEastMin)/kDemStep;
    const double v=(kDemNorthMax-north)/kDemStep;
    if (u<0 || v<0 || u>kDemWidth-1 || v>kDemHeight-1) return false;
    const int x=std::min(static_cast<int>(std::floor(u)),kDemWidth-2);
    const int y=std::min(static_cast<int>(std::floor(v)),kDemHeight-2);
    const float fx=static_cast<float>(u-x), fy=static_cast<float>(v-y);
    const auto index=static_cast<std::size_t>(y*kDemWidth+x);
    const float a=kDemRelativeMeters[index],b=kDemRelativeMeters[index+1];
    const float c=kDemRelativeMeters[index+kDemWidth],d=kDemRelativeMeters[index+kDemWidth+1];
    out=(1-fy)*((1-fx)*a+fx*b)+fy*((1-fx)*c+fx*d);
    return true;
}
struct Point3 { float x,y,z; };
inline bool TryTerrainGridVertex(int row,int col,Point3& out) {
    if(row<0||col<0||row>=kDemHeight||col>=kDemWidth) return false;
    const float east=kDemEastMin+col*kDemStep;
    const float north=kDemNorthMax-row*kDemStep;
    const Point p=EastNorthToGame(east,north);
    out={p.x,kDemRelativeMeters[static_cast<std::size_t>(row*kDemWidth+col)],p.z};
    return true;
}

struct Triangle { Point a,b,c; }; // clockwise XZ => upward +Y surface normal
inline double Cross2(Point a,Point b,Point c) {
    return (static_cast<double>(b.x)-a.x)*(c.z-a.z)-
           (static_cast<double>(b.z)-a.z)*(c.x-a.x);
}
inline bool InsideTriangleCCW(Point p,Point a,Point b,Point c) {
    constexpr double eps=1e-7;
    return Cross2(a,b,p)>=-eps && Cross2(b,c,p)>=-eps && Cross2(c,a,p)>=-eps;
}

// Simple-ring ear clipping for rendering course/context polygons.
// Holes are separate source features; draw/subtract separately. No fan of concave rings.
inline bool TriangulateSimplePolygon(const Point* points,std::size_t count,
                                     std::vector<Triangle>& out) {
    out.clear();
    if(count<3) return false;
    std::vector<Point> ring;
    for(std::size_t i=0;i<count;++i) {
        if(!std::isfinite(points[i].x)||!std::isfinite(points[i].z)) return false;
        if(ring.empty()||!SamePoint(ring.back(),points[i])) ring.push_back(points[i]);
    }
    if(ring.size()>1&&SamePoint(ring.front(),ring.back())) ring.pop_back();
    if(ring.size()<3) return false;
    double area=0;
    for(std::size_t i=0,j=ring.size()-1;i<ring.size();j=i++)
        area+=static_cast<double>(ring[j].x)*ring[i].z-
              static_cast<double>(ring[i].x)*ring[j].z;
    if(std::fabs(area)<1e-7) return false;
    if(area<0) std::reverse(ring.begin(),ring.end());
    std::vector<std::size_t> indices;
    for(std::size_t i=0;i<ring.size();++i) indices.push_back(i);
    while(indices.size()>3) {
        bool clipped=false;
        for(std::size_t i=0;i<indices.size();++i) {
            const auto prev=indices[(i+indices.size()-1)%indices.size()];
            const auto curr=indices[i],next=indices[(i+1)%indices.size()];
            const auto a=ring[prev],b=ring[curr],c=ring[next];
            if(Cross2(a,b,c)<=1e-7) continue;
            bool contains=false;
            for(const auto k:indices)
                if(k!=prev&&k!=curr&&k!=next&&InsideTriangleCCW(ring[k],a,b,c)) {
                    contains=true;break;
                }
            if(contains) continue;
            out.push_back({a,c,b});
            indices.erase(indices.begin()+static_cast<std::ptrdiff_t>(i));
            clipped=true;break;
        }
        if(!clipped) {out.clear();return false;}
    }
    const auto a=ring[indices[0]],b=ring[indices[1]],c=ring[indices[2]];
    if(Cross2(a,b,c)<=1e-7) {out.clear();return false;}
    out.push_back({a,c,b});
    return true;
}

} // namespace dobong_source
