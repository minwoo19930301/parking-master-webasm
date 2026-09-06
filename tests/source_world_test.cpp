#include "src/dobong/dobong_source_queries.h"
#include "src/dobong/context_geometry.h"
#include "src/dobong/exterior_observations.h"
#include <cassert>
#include <iostream>
#include <limits>

int main() {
    using namespace dobong_source;
    assert(kCourseFeatures.size()==24);
    assert(std::strcmp(kCourseCaptureDate,"2025-03-22")==0);
    assert(kObservedParkingAccessCount==4);
    assert(kHasHistoricalOfficial2023RouteTopology);
    assert(kContextFeatures.size()>=54);
    assert(kDemRelativeMeters.size()==129*129);
    const auto origin=Wgs84ToGame(kOriginLongitude,kOriginLatitude);
    assert(origin.x==52 && origin.z==-60);
    assert(!kHasVerifiedStartLine && !kHasVerifiedFinishLine && !kHasVerifiedSlopeProfile && !kHasVerifiedRouteSequence);
    // Current default spawn and first 28m of northward lane must fit the full car.
    constexpr float angle=-1.842f;
    for(float distance=0;distance<28;distance+=.25f) {
        const Point centre{39+std::cos(angle)*distance,30+std::sin(angle)*distance};
        for(float x:{-2.225f,0.f,2.225f})for(float z:{-.91f,0.f,.91f}) {
            const Point p{centre.x+x*std::cos(angle)-z*std::sin(angle),centre.z+x*std::sin(angle)+z*std::cos(angle)};
            assert(IsObservedRoadSurface(p,true,false));
        }
    }
    assert(!IsObservedRoadSurface({20,20},true,false)); // parked vehicle apron, not a road
    assert(!IsObservedRoadSurface(CourseImagePixelToGame(322,494),true,false)); // island
    assert(!IsObservedRoadSurface(CourseImagePixelToGame(359,630),true,false)); // untraced parking complex
    for(const Point pixel: {Point{367,617},Point{366,692},Point{394,693},Point{385,759}}) {
        const auto p=CourseImagePixelToGame(pixel.x,pixel.z);
        assert(InObservedParkingAccess(p));
        assert(IsObservedRoadSurface(p,true,false));
        assert(!IsObservedRoadSurface(p,false,false));
    }
    assert(!IsObservedRoadSurface(CourseImagePixelToGame(484,348),true,false));
    assert(!IsObservedRoadSurface(CourseImagePixelToGame(331,813),true,false));
    assert(!FindCourseFeature("parking-bay-gap-north-candidate"));
    std::vector<Triangle> triangles;
    std::size_t count=0;
    for(const auto& f:kCourseFeatures) {
        assert(f.first+f.count<=kCourseVertices.size());
        if(f.quality==Quality::U)assert(f.count==0);
        if(f.geometry==GeometryType::Polygon) {
            assert(TriangulateSimplePolygon(kCourseVertices.data()+f.first,f.count,triangles));
            for(const auto& t:triangles)assert(Cross2(t.a,t.b,t.c)<0);
            count+=triangles.size();
        }
    }
    bool mainHall=false,indoorSubstation=false;
    std::size_t buildingShells=0;
    for(const auto& f:kContextFeatures) {
        if(std::strcmp(f.id,"way/380676074")==0)mainHall=true;
        assert(f.first+f.count<=kContextVertices.size());
        if(f.geometry==GeometryType::Polygon) {
            assert(TriangulateSimplePolygon(kContextVertices.data()+f.first,f.count,triangles));
            for(const auto& t:triangles)assert(Cross2(t.a,t.b,t.c)<0);
            count+=triangles.size();
        }
        if(HasBuildingShell(f)) {
            ++buildingShells;
            if(std::strcmp(f.id,"way/472066007")==0) {
                indoorSubstation=true;
                assert(f.kind==ContextKind::Substation);
                assert(f.heightMeters==-1 && f.buildingLevels==-1);
                auto outdoor=f;
                outdoor.id="unverified-outdoor-substation";
                assert(!HasBuildingShell(outdoor));
                auto nonPolygon=f;
                nonPolygon.geometry=GeometryType::Point;
                assert(!HasBuildingShell(nonPolygon));
            }
            const auto* pts=kContextVertices.data()+f.first;
            const float outward=OutwardNormalSign(pts,f.count);
            for(std::size_t i=1;i<f.count;++i) {
                const auto a=pts[i-1],b=pts[i];
                const float length=std::hypot(b.x-a.x,b.z-a.z);
                if(length<2.f)continue;
                const float dx=(b.x-a.x)/length,dz=(b.z-a.z)/length;
                const Point windowMid{(a.x+b.x)*.5f-dz*.035f*outward,(a.z+b.z)*.5f+dx*.035f*outward};
                assert(!ContainsPolygon(pts,f.count,windowMid));
            }
        }
    }
    assert(mainHall);
    assert(indoorSubstation && buildingShells==36);
    // Observations attach to the exact five existing OSM buildings only.
    assert(kRoofObservations.size()==5);
    for(const auto& roof:kRoofObservations) {
        std::size_t matches=0;
        for(const auto& f:kContextFeatures) if(std::strcmp(f.id,roof.id)==0) {
            ++matches;
            assert(f.kind==ContextKind::Building);
            assert(f.heightMeters==-1 && f.buildingLevels==-1); // missing-data sentinel, not newly measured
        }
        assert(matches==1);
        assert(FindRoofObservation(roof.id)==&roof);
    }
    assert(!FindRoofObservation(nullptr));
    assert(!FindRoofObservation("unknown"));
    assert(!FindRoofObservation("aerial-2025-ordinary-se-teal-roof"));
    assert(!FindRoofObservation("way/1180013352")); // tower keeps prior materials
    for(float v:kDemRelativeMeters)assert(std::isfinite(v));
    float height=123;
    assert(!TrySampleTerrain({9000,0},height)&&height==123);
    assert(!TrySampleTerrain({std::numeric_limits<float>::quiet_NaN(),0},height));
    assert(TrySampleTerrain({52,-60},height));
    assert(std::fabs(height-kDemRelativeMeters[64*129+64])<.001f);
    std::cout<<"PASS source world: full-car spawn corridor, surface classes, main hall, "<<count<<" polygon triangles, 16641 DEM samples\n";
}
