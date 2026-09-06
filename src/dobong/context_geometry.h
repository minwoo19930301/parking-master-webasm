#pragma once
#include "dobong_source_world.h"
#include <cstring>

namespace dobong_source {
inline bool HasBuildingShell(const ContextFeature& feature) {
    if(feature.geometry!=GeometryType::Polygon)return false;
    // The saved source tags this particular substation building=yes and
    // location=indoor. Its power classification must not hide its footprint.
    // Do not turn arbitrary outdoor substations into invented buildings.
    return feature.kind==ContextKind::Building ||
        (feature.kind==ContextKind::Substation && feature.id &&
         std::strcmp(feature.id,"way/472066007")==0);
}
// Ring order differs between OSM buildings. Return the sign multiplying the
// left edge normal (-dz,+dx) to point OUT of either clockwise or CCW rings.
inline float OutwardNormalSign(const Point* points,std::size_t count) {
    if(count<3)return 1.f;
    double area=0;
    for(std::size_t i=0,j=count-1;i<count;j=i++)
        area+=static_cast<double>(points[j].x)*points[i].z-
              static_cast<double>(points[i].x)*points[j].z;
    return area>0?-1.f:1.f;
}
} // namespace dobong_source
