#pragma once
#include "dobong/road_ribbons.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace road_driving {
using dobong_road_source::Point;
inline float Distance(Point a,Point b){return std::hypot(a.x-b.x,a.z-b.z);}
struct Pose { Point position; float heading; };
inline bool Overlap(Pose a,Point halfA,Pose b,Point halfB){
    const Point axesA[]={{std::cos(a.heading),std::sin(a.heading)},{-std::sin(a.heading),std::cos(a.heading)}};
    const Point axesB[]={{std::cos(b.heading),std::sin(b.heading)},{-std::sin(b.heading),std::cos(b.heading)}};
    auto dot=[](Point p,Point q){return p.x*q.x+p.z*q.z;};
    const Point delta{b.position.x-a.position.x,b.position.z-a.position.z};
    for(const auto axis:{axesA[0],axesA[1],axesB[0],axesB[1]}){
        const float radiusA=halfA.x*std::fabs(dot(axesA[0],axis))+halfA.z*std::fabs(dot(axesA[1],axis));
        const float radiusB=halfB.x*std::fabs(dot(axesB[0],axis))+halfB.z*std::fabs(dot(axesB[1],axis));
        if(std::fabs(dot(delta,axis))>radiusA+radiusB)return false;
    }
    return true;
}
struct Path {
    std::vector<Point> points;
    std::vector<float> distances;
    Path()=default;
    Path(const Point* p,std::size_t n):points(p,p+n),distances(n,0) {
        for(std::size_t i=1;i<n;++i)distances[i]=distances[i-1]+Distance(points[i-1],points[i]);
    }
    float Length()const{return distances.empty()?0:distances.back();}
    Pose Sample(float s,float lateral=0)const {
        if(points.size()<2)return {{0,0},0};
        s=std::clamp(s,0.f,Length());
        auto it=std::upper_bound(distances.begin(),distances.end(),s);
        std::size_t i=std::clamp<std::size_t>(it-distances.begin(),1,points.size()-1);
        const auto a=points[i-1],b=points[i];
        const float len=std::max(.0001f,distances[i]-distances[i-1]),t=(s-distances[i-1])/len;
        const float dx=(b.x-a.x)/len,dz=(b.z-a.z)/len;
        return {{a.x+(b.x-a.x)*t-dz*lateral,a.z+(b.z-a.z)*t+dx*lateral},std::atan2(dz,dx)};
    }
    float Nearest(Point p,float from,float to,float* separation=nullptr,const Point* direction=nullptr)const {
        float best=1e20f,found=std::clamp(from,0.f,Length());
        for(std::size_t i=1;i<points.size();++i){
            if(distances[i]<from||distances[i-1]>to)continue;
            const auto a=points[i-1],b=points[i];const float dx=b.x-a.x,dz=b.z-a.z;
            const float len=distances[i]-distances[i-1];if(len<.001f)continue;
            if(direction && dx*direction->x+dz*direction->z<0)continue;
            const float lo=std::clamp((from-distances[i-1])/len,0.f,1.f),hi=std::clamp((to-distances[i-1])/len,0.f,1.f);
            const float t=std::clamp(((p.x-a.x)*dx+(p.z-a.z)*dz)/(len*len),lo,hi);
            const float d=Distance(p,{a.x+dx*t,a.z+dz*t});
            if(d<best-.0001f){best=d;found=distances[i-1]+t*len;}
        }
        if(separation)*separation=best;
        return found;
    }
};
struct Progress {
    float distance=0,separation=0;
    bool complete=false;
    Point previous{};bool hasPrevious=false;
    void Update(const Path& path,Point p,float travelled,float speed) {
        if(complete)return;
        // Only search the physically reachable local arc. Repeated junctions/start-end
        // overlap must never jump to a later lap or finish at the starting point.
        const Point movement{p.x-previous.x,p.z-previous.z};
        const bool moving=hasPrevious&&Distance(p,previous)>.001f;
        const float next=path.Nearest(p,std::max(0.f,distance-12),std::min(path.Length(),distance+std::max(12.f,travelled*2+4)),&separation,moving?&movement:nullptr);
        previous=p;hasPrevious=true;
        if(separation<12)distance=std::max(distance,next);
        if(distance>=path.Length()-5 && Distance(p,path.points.back())<7 && std::fabs(speed)<.6f)complete=true;
    }
};
inline bool OnRoad(Point p) {
    static const auto paths=[](){std::vector<Path> result;for(const auto& road:dobong_road_source::kRoads)result.emplace_back(road.points,road.count);return result;}();
    std::size_t index=0;
    for(const auto& road:dobong_road_source::kRoads){
        const auto& path=paths[index++];float separation;
        path.Nearest(p,0,path.Length(),&separation);
        if(separation<=road.width*.5f)return true;
    }
    // Official turn anchors, with disclosed unmeasured practice maneuver areas.
    for(const auto& route:dobong_road_source::kRoutes)
        for(std::size_t m=0;m<route.maneuverCount;++m)
            if(Distance(p,route.points[route.maneuvers[m].vertex])<13)return true;
    static const auto routes=[](){std::vector<Path> result;for(const auto& route:dobong_road_source::kRoutes)result.emplace_back(route.points,route.count);return result;}();
    for(const auto& path:routes){float separation;path.Nearest(p,0,path.Length(),&separation);if(separation<=4.5f)return true;}
    return false;
}
struct Vehicle { int road=0,direction=1,color=0,style=0;float distance=0,speed=0,cruise=0;Pose pose{};bool active=false; };
class Traffic {
    std::uint32_t seed_=1;
    std::vector<Path> paths_;
    float Random(){seed_^=seed_<<13;seed_^=seed_>>17;seed_^=seed_<<5;return (seed_&0xffffff)/16777216.f;}
    struct Connection {int road=-1,direction=1;};
    Connection Next(const Vehicle& vehicle)const {
        const auto& path=paths_[vehicle.road];
        const auto end=path.Sample(vehicle.direction>0?path.Length():0);
        const float heading=end.heading+(vehicle.direction<0?3.14159265f:0);
        float best=-.1f;Connection result;
        for(std::size_t i=0;i<paths_.size();++i){
            if(static_cast<int>(i)==vehicle.road)continue;
            for(int direction:{1,-1}){
                if(direction<0&&dobong_road_source::kRoads[i].oneWay)continue;
                const auto start=paths_[i].Sample(direction>0?0:paths_[i].Length());
                if(Distance(end.position,start.position)>.8f)continue;
                const float score=std::cos(start.heading+(direction<0?3.14159265f:0)-heading);
                if(score>best){best=score;result={static_cast<int>(i),direction};}
            }
        }
        return result;
    }
    Pose At(const Vehicle& v)const {
        const auto& road=dobong_road_source::kRoads[v.road];
        auto pose=paths_[v.road].Sample(v.distance,road.oneWay?0.f:3.f*v.direction);
        if(v.direction<0)pose.heading+=3.14159265f;
        return pose;
    }
    void Spawn(Vehicle& v,Point player) {
        v.active=false;
        for(int attempt=0;attempt<80;++attempt){
            v.road=static_cast<int>(Random()*paths_.size());
            const float length=paths_[v.road].Length();if(length<100)continue;
            v.direction=dobong_road_source::kRoads[v.road].oneWay?1:(Random()<.5f?-1:1);
            v.distance=25+Random()*(length-50);v.pose=At(v);
            if(Distance(v.pose.position,player)<90)continue;
            bool blocked=false;for(const auto& other:vehicles)if(&other!=&v&&other.active&&Distance(v.pose.position,other.pose.position)<18){blocked=true;break;}
            if(blocked)continue;
            v.cruise=6+Random()*4;v.speed=0;v.color=static_cast<int>(Random()*6);v.style=static_cast<int>(Random()*3);v.active=true;return;
        }
        v.cruise=0;v.speed=0; // Do not force an unsafe spawn when no clear slot exists.
    }
public:
    std::vector<Vehicle> vehicles;
    void Reset(std::uint32_t seed,Point player) {
        seed_=seed?seed:1;paths_.clear();vehicles.clear();
        for(const auto& road:dobong_road_source::kRoads)paths_.emplace_back(road.points,road.count);
        vehicles.resize(36);for(auto& v:vehicles)Spawn(v,player);
    }
    void Update(float dt,Point player,float playerHeading=0) {
        if(!(dt>0))return;
        dt=std::min(dt,.05f);
        const auto previous=vehicles;
        for(std::size_t i=0;i<vehicles.size();++i){
            auto& v=vehicles[i];
            if(!v.active){Spawn(v,player);continue;}
            const float end=v.direction>0?paths_[v.road].Length()-v.distance:v.distance;
            const auto connection=Next(v);
            if(((end<12&&connection.road<0)||v.cruise==0)&&Distance(v.pose.position,player)>160){Spawn(v,player);continue;}
            float gap=connection.road>=0?10000:end;
            const float fx=std::cos(v.pose.heading),fz=std::sin(v.pose.heading);
            auto obstacle=[&](Point p){const float dx=p.x-v.pose.position.x,dz=p.z-v.pose.position.z;const float ahead=dx*fx+dz*fz,lateral=std::fabs(-dx*fz+dz*fx);if(ahead>0&&lateral<2.7f)gap=std::min(gap,ahead-5.5f);};
            obstacle(player);for(std::size_t j=0;j<previous.size();++j)if(i!=j&&previous[j].active)obstacle(previous[j].pose.position);
            const float target=std::min(v.cruise,std::sqrt(std::max(0.f,2*3.f*(gap-3))));
            v.speed=std::clamp(target,v.speed-7*dt,v.speed+2*dt);
            const float advance=std::min(v.speed*dt,std::max(0.f,gap-1));
            auto proposed=v;
            if(advance>=end&&connection.road>=0){
                proposed.road=connection.road;proposed.direction=connection.direction;
                const float excess=advance-end;
                proposed.distance=proposed.direction>0?excess:paths_[proposed.road].Length()-excess;
            }else proposed.distance=std::clamp(v.distance+advance*v.direction,0.f,paths_[v.road].Length());
            const auto pose=At(proposed);
            // Test full oriented bodies, including a player parked sideways. Check
            // intermediate poses across source-way joins as well as the endpoint.
            bool blocked=false;
            const float angleDelta=std::remainder(pose.heading-v.pose.heading,6.2831853f);
            const int steps=std::max(1,static_cast<int>(std::ceil(Distance(pose.position,v.pose.position)/.25f)));
            for(int step=1;step<=steps&&!blocked;++step){
                const float t=static_cast<float>(step)/steps;
                const Pose sample{{v.pose.position.x+(pose.position.x-v.pose.position.x)*t,v.pose.position.z+(pose.position.z-v.pose.position.z)*t},v.pose.heading+angleDelta*t};
                blocked=Overlap(sample,{2.4f,1.f},{player,playerHeading},{2.225f,.91f});
                for(std::size_t j=0;j<previous.size();++j){
                    const auto& other=j<i?vehicles[j]:previous[j];
                    if(i!=j&&other.active&&Overlap(sample,{2.4f,1.f},other.pose,{2.4f,1.f}))blocked=true;
                }
            }
            if(blocked)v.speed=0;else {v=proposed;v.pose=pose;}
        }
    }
};
} // namespace road_driving
