#include "src/driving_camera.h"
#include <cassert>
#include <iostream>

int main() {
    for(float heading:{-.78f,.78f,1.57f,2.4f,3.14f}){
        const float glAngle=driving_camera::RenderYawDegrees(heading)*3.14159265358979323846f/180.f;
        assert(std::fabs(std::cos(glAngle)-std::cos(heading))<.00001f);
        assert(std::fabs(-std::sin(glAngle)-std::sin(heading))<.00001f);
    }
    for(float aspect:{.36f,.56f,1.f,1.78f,3.55f})for(float angle:{0.f,1.57f,3.14f,-1.57f}) {
        const auto camera=driving_camera::Chase(42,-80,angle,13,50,aspect);
        const float fx=std::cos(angle),fz=std::sin(angle);
        assert((camera.position.x-42)*fx+(camera.position.z+80)*fz<-8);
        assert((camera.target.x-42)*fx+(camera.target.z+80)*fz>1.9f);
        assert(camera.position.y>17 && camera.target.y>14);
        assert(camera.fovy==60);
    }
    for(int fps:{20,30,60,120}) {
        float position=0;
        for(int frame=0;frame<fps;++frame)position+=(10-position)*driving_camera::FollowBlend(1.f/fps);
        assert(std::fabs(position-10*(1-std::exp(-8.f)))<.00001f);
    }
    assert(driving_camera::FollowBlend(0)==0);
    std::cout<<"PASS chase camera: heading/portrait/ground clearance and frame-independent follow\n";
}
