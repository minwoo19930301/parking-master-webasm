#include "src/road_driving.h"
#include "src/vehicle_physics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace road_driving;
struct Car {Point position;float heading=0,speed=0,steering=0;};
float norm(float a){return std::remainder(a,6.28318530718f);}
bool fits(const Car& car){
    float c=std::cos(car.heading),s=std::sin(car.heading);
    for(float x:{-2.225f,2.225f})for(float z:{-.91f,.91f})
        if(!OnRoad({car.position.x+c*x-s*z,car.position.z+s*x+c*z}))return false;
    return true;
}
// Exact numerical update from main.cpp UpdateVehicle + road-mode rollback.
// D gear, engine on, belt on, parking brake off; no NPCs in this first probe.
bool tick(Car& car,float dt,float steer,float throttle,float brake){
    const Car previous=car;
    const float targetSteering=steer*.66f;
    car.steering=car.steering+(targetSteering-car.steering)*dt*7.f;
    float desired=driving_physics::UpdateSpeed(car.speed,dt,false,throttle,brake,16.7f);
    car.speed=std::clamp(desired,-3.2f,16.7f);
    const float turnRate=std::tan(car.steering)*car.speed/2.72f;
    car.heading=norm(car.heading+turnRate*dt);
    car.position.x+=std::cos(car.heading)*car.speed*dt;
    car.position.z+=std::sin(car.heading)*car.speed*dt;
    if(!fits(car)){car=previous;car.speed=0;return false;}
    return true;
}
int main(int argc,char**argv){
    float look=argc>1?std::atof(argv[1]):5.5f;
    float cruising=argc>2?std::atof(argv[2]):4.f;
    bool all=true;
    for(const auto& route:dobong_road_source::kRoutes){
        Path path(route.points,route.count);const auto spawn=path.Sample(0);
        Car car{spawn.position,spawn.heading,0,0};Progress progress;
        int collisions=0,frames=0;float stuck=0,lastProgress=0,maxSteer=0,travelled=0;
        float sign=1;
        for(;frames<60*2400&&!progress.complete;++frames){
            const float dt=1.f/60;
            auto target=path.Sample(progress.distance+look);
            float dx=target.position.x-car.position.x,dz=target.position.z-car.position.z;
            float d=std::hypot(dx,dz),angle=norm(std::atan2(dz,dx)-car.heading);
            if(std::fabs(angle)>.03f&&std::fabs(angle)<3.f)sign=angle>0?1:-1;
            float steer=std::atan2(2*2.72f*std::sin(angle),std::max(d,1.f))/.66f;
            // A centreline U-turn is an exact reversal. Choose a turning side
            // instead of requesting zero curvature from sin(pi). Actual car yaw
            // remains constrained by steering response; no snap/teleport.
            if(std::fabs(angle)>1.35f)steer=sign;
            steer=std::clamp(steer,-1.f,1.f);
            float targetSpeed=cruising;
            if(progress.distance>path.Length()-8&&Distance(car.position,path.points.back())<5)targetSpeed=0;
            float throttle=car.speed<targetSpeed?std::clamp((targetSpeed-car.speed)*2.f+.1f,0.f,1.f):0;
            float brake=car.speed>=targetSpeed?std::clamp((car.speed-targetSpeed)*3.f+.1f,0.f,1.f):0;
            auto before=car.position;
            if(!tick(car,dt,steer,throttle,brake))++collisions;
            float delta=Distance(before,car.position);travelled+=delta;
            progress.Update(path,car.position,delta,car.speed);
            maxSteer=std::max(maxSteer,std::fabs(car.steering));
            if(progress.distance>lastProgress+.1f){stuck=0;lastProgress=progress.distance;}else stuck+=dt;
            if(stuck>30)break;
        }
        all&=progress.complete&&collisions==0&&maxSteer<=.6601f&&std::isfinite(travelled)&&travelled>path.Length()-30;
        std::cout<<(progress.complete?"PASS ":"STUCK ")<<route.id<<" look="<<look<<" cruise="<<cruising<<" seconds="<<frames/60.f<<" progress="<<progress.distance<<"/"<<path.Length()<<" travelled="<<travelled<<" collisionRollbacks="<<collisions<<" steeringMax="<<maxSteer<<" position="<<car.position.x<<","<<car.position.z<<" heading="<<car.heading<<" speed="<<car.speed<<" separation="<<progress.separation<<std::endl;
    }
    return all?0:2;
}
