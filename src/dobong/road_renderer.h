#pragma once
#include "source_renderer.h"
#include "../road_driving.h"
namespace dobong_visual {
class RoadWorld {
    Model model_{};bool ready_=false;
public:
    void Init(){
        Builder surface,marking;
        for(const auto& route:dobong_road_source::kRoutes)for(std::size_t i=1;i<route.count;++i){
            const auto a=route.points[i-1],b=route.points[i];
            surface.Ribbon({a.x,a.z},{b.x,b.z},9,.075f,{68,76,80,255});
        }
        for(const auto& road:dobong_road_source::kRoads){
            for(std::size_t i=1;i<road.count;++i){
                const auto a=road.points[i-1],b=road.points[i];
                surface.Ribbon({a.x,a.z},{b.x,b.z},road.width+.8f,.06f,{178,184,177,255});
                surface.Ribbon({a.x,a.z},{b.x,b.z},road.width,.08f,{68,76,80,255});
            }
        }
        // These open maneuver pads disclose estimated U-turn space, not stop lines.
        for(const auto& route:dobong_road_source::kRoutes)for(std::size_t m=0;m<route.maneuverCount;++m){
            const auto p=route.points[route.maneuvers[m].vertex];
            for(int k=0;k<24;++k){const float a=k*6.2831853f/24,b=(k+1)*6.2831853f/24;
                surface.Triangle({p.x,.085f,p.z},{p.x+13*std::cos(a),.085f,p.z+13*std::sin(a)},{p.x+13*std::cos(b),.085f,p.z+13*std::sin(b)},{68,76,80,255});
            }
        }
        for(const auto& road:dobong_road_source::kRoads){
            road_driving::Path path(road.points,road.count);
            for(float s=3;s<path.Length()-3;s+=10){
                const auto a=path.Sample(s).position,b=path.Sample(std::min(s+4,path.Length())).position;
                marking.Ribbon({a.x,a.z},{b.x,b.z},.12f,.10f,road.oneWay?Color{221,225,218,255}:Color{235,188,65,255});
            }
        }
        surface.vertices.insert(surface.vertices.end(),marking.vertices.begin(),marking.vertices.end());
        surface.colors.insert(surface.colors.end(),marking.colors.begin(),marking.colors.end());
        model_=surface.Upload();ready_=true;
    }
    void Draw()const{
        DrawPlane({300,-.05f,-250},{4500,5500},{102,119,103,255});
        if(ready_){rlDisableBackfaceCulling();DrawModel(model_,{0,0,0},1,WHITE);rlEnableBackfaceCulling();}
    }
    void Unload(){if(ready_)UnloadModel(model_);ready_=false;}
};
}
