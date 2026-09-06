#include "src/road_driving.h"
#include "src/vehicle_physics.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace road_driving;
    for(const auto& route:dobong_road_source::kRoutes){
        Path path(route.points,route.count);Progress progress;
        assert(path.Length()>5500&&path.Length()<6000);
        progress.Update(path,path.points.front(),0,0);assert(!progress.complete&&progress.distance<1);
        // Four complete ordered paths, repeated road segments and U-turns included.
        for(float s=0;s<path.Length();s+=1){auto p=path.Sample(s);progress.Update(path,p.position,1,4);assert(std::abs(progress.distance-s)<2);if(!OnRoad(p.position)){std::cerr<<route.id<<" gap at "<<s<<" x="<<p.position.x<<" z="<<p.position.z<<"\n";return 1;}}
        progress.Update(path,path.points.back(),1,0);assert(progress.complete);
        Progress skipped;skipped.Update(path,path.Sample(3000).position,1,1);assert(skipped.distance<20&&!skipped.complete);
    }
    Traffic a,b;const Point player{25.587f,-188.084f};a.Reset(42,player);b.Reset(42,player);
    int active=0;
    for(std::size_t i=0;i<a.vehicles.size();++i){const auto& v=a.vehicles[i];if(!v.active)continue;++active;assert(Distance(v.pose.position,player)>=90);assert(v.distance==b.vehicles[i].distance);}
    assert(active>=20);
    Traffic connected;connected.Reset(7,{0,0});connected.vehicles.resize(1);
    auto& vehicle=connected.vehicles[0];
    const auto& source=dobong_road_source::kRoads[6];Path joined(source.points,source.count);
    vehicle.road=6;vehicle.direction=1;vehicle.distance=joined.Length()-.2f;vehicle.pose=joined.Sample(vehicle.distance,source.oneWay?0:3);
    vehicle.cruise=8;vehicle.speed=8;vehicle.active=true;
    const Point spectator{vehicle.pose.position.x+40,vehicle.pose.position.z+40};
    for(int i=0;i<120;i++)connected.Update(1.f/60,spectator);
    assert(vehicle.road!=6); // An OSM way split is not a traffic stop line.
    const auto paused=a.vehicles;a.Update(0,player);for(std::size_t i=0;i<paused.size();++i)assert(paused[i].distance==a.vehicles[i].distance);
    for(int step=0;step<3600;++step){a.Update(1.f/60,player);for(const auto& v:a.vehicles)if(v.active){assert(std::isfinite(v.pose.position.x));assert(v.speed>=0&&v.speed<=10);assert(!Overlap(v.pose,{2.4f,1.f},{player,0},{2.225f,.91f}));}}
    Traffic crossing;crossing.Reset(1,{0,0});crossing.vehicles.resize(1);
    auto& npc=crossing.vehicles[0];const auto& first=dobong_road_source::kRoads[0];Path firstPath(first.points,first.count);
    npc.road=0;npc.direction=1;npc.distance=100;npc.pose=firstPath.Sample(100);npc.speed=10;npc.cruise=10;npc.active=true;
    const float fx=std::cos(npc.pose.heading),fz=std::sin(npc.pose.heading);
    const Point parked{npc.pose.position.x+fx*3.6333f-fz*3.1f,npc.pose.position.z+fz*3.6333f+fx*3.1f};
    const float parkedHeading=npc.pose.heading+1.5707963f;
    assert(!Overlap(npc.pose,{2.4f,1.f},{parked,parkedHeading},{2.225f,.91f}));
    crossing.Update(1.f/30,parked,parkedHeading);
    assert(!Overlap(npc.pose,{2.4f,1.f},{parked,parkedHeading},{2.225f,.91f}));
    float speed=0;for(int i=0;i<600;++i)speed=driving_physics::UpdateSpeed(speed,1.f/60,false,1,0,16.7f);
    assert(speed>16&&speed<=16.7f);
    std::cout<<"ABCD full-route progress, no shortcut, road coverage, traffic seed/pause/spacing and road speed PASS\n";
}
