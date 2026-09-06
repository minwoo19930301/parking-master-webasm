#pragma once
#include "dobong_source_queries.h"
#include "context_geometry.h"
#include "exterior_observations.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace dobong_visual {
using dobong_source::Point;
inline Vector3 V(Point p,float y=0) { return {p.x,y,p.z}; }
inline Color Tint(Color c,float amount) {
    return {static_cast<unsigned char>(c.r*amount),static_cast<unsigned char>(c.g*amount),static_cast<unsigned char>(c.b*amount),c.a};
}
struct Builder {
    std::vector<Vector3> vertices;
    std::vector<Color> colors;
    void Triangle(Vector3 a,Vector3 b,Vector3 c,Color color) {
        vertices.insert(vertices.end(),{a,b,c}); colors.insert(colors.end(),3,color);
    }
    void Quad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,Color color) { Triangle(a,b,c,color);Triangle(a,c,d,color); }
    void Polygon(const Point* points,std::size_t count,float y,Color color) {
        std::vector<dobong_source::Triangle> triangles;
        if(!dobong_source::TriangulateSimplePolygon(points,count,triangles)) {
            TraceLog(LOG_ERROR,"Invalid source polygon; refusing guessed triangulation"); return;
        }
        for(const auto& t:triangles) Triangle(V(t.a,y),V(t.b,y),V(t.c,y),color);
    }
    void Ribbon(Point a,Point b,float width,float y,Color color) {
        const float dx=b.x-a.x,dz=b.z-a.z,len=std::hypot(dx,dz);
        if(len<.001f) return;
        const Point n{-dz/len*width*.5f,dx/len*width*.5f};
        Quad({a.x+n.x,y,a.z+n.z},{b.x+n.x,y,b.z+n.z},{b.x-n.x,y,b.z-n.z},{a.x-n.x,y,a.z-n.z},color);
    }
    void Extrude(const Point* points,std::size_t count,float base,float height,Color color,const dobong_source::RoofObservation* roof=nullptr) {
        Polygon(points,count,base+height,roof?Color{roof->r,roof->g,roof->b,255}:Tint(color,.92f));
        for(std::size_t i=1;i<count;++i) {
            const auto a=points[i-1],b=points[i];
            const float shade=.75f+.20f*std::fabs(b.x-a.x)/std::max(.01f,std::hypot(b.x-a.x,b.z-a.z));
            Quad(V(a,base),V(b,base),V(b,base+height),V(a,base+height),Tint(color,shade));
        }
    }
    Model Upload() const {
        Mesh mesh{}; mesh.vertexCount=static_cast<int>(vertices.size()); mesh.triangleCount=mesh.vertexCount/3;
        mesh.vertices=static_cast<float*>(MemAlloc(vertices.size()*3*sizeof(float)));
        mesh.colors=static_cast<unsigned char*>(MemAlloc(colors.size()*4));
        for(std::size_t i=0;i<vertices.size();++i) {
            mesh.vertices[3*i]=vertices[i].x;mesh.vertices[3*i+1]=vertices[i].y;mesh.vertices[3*i+2]=vertices[i].z;
            mesh.colors[4*i]=colors[i].r;mesh.colors[4*i+1]=colors[i].g;mesh.colors[4*i+2]=colors[i].b;mesh.colors[4*i+3]=255;
        }
        UploadMesh(&mesh,false); return LoadModelFromMesh(mesh);
    }
};

// The DEM cannot resolve kerbs/ramps. A clearly documented flat near-site datum
// blends into source relief over 500–900m; no synthetic background mountains.
inline float BackgroundElevation(Point p) {
    float height=0;
    if(!dobong_source::TrySampleTerrain(p,height)) return 0;
    const float blend=std::clamp((std::hypot(p.x,p.z)-500.f)/400.f,0.f,1.f);
    return height*blend;
}

class SourceWorld {
    Model terrain_{},context_{},course_{};
    bool ready_=false;
public:
    void Init() {
        using namespace dobong_source;
        Builder terrain,context,course;
        for(int row=0;row<kDemHeight-1;++row) for(int col=0;col<kDemWidth-1;++col) {
            Point3 a{},b{},c{},d{};
            TryTerrainGridVertex(row,col,a);TryTerrainGridVertex(row+1,col,b);
            TryTerrainGridVertex(row,col+1,c);TryTerrainGridVertex(row+1,col+1,d);
            const auto adjust=[](Point3 p){return Vector3{p.x,BackgroundElevation({p.x,p.z})-.08f,p.z};};
            const Vector3 av=adjust(a),bv=adjust(b),cv=adjust(c),dv=adjust(d);
            const Vector3 normal=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(bv,av),Vector3Subtract(cv,av)));
            const float light=.72f+.25f*std::max(0.f,Vector3DotProduct(normal,Vector3Normalize({-.4f,1.f,.2f})));
            const float haze=std::clamp(std::hypot(a.x,a.z)/14000.f,0.f,.75f);
            const Color green{static_cast<unsigned char>(83+(153-83)*haze),static_cast<unsigned char>(112+(176-112)*haze),static_cast<unsigned char>(83+(189-83)*haze),255};
            terrain.Triangle(av,bv,cv,Tint(green,light));terrain.Triangle(cv,bv,dv,Tint(green,light));
        }
        // OSM footprints and centrelines are fixed, with no invented apartment ring.
        // Missing vertical/facade data are explicitly illustrative, see provenance.
        for(const auto& f:kContextFeatures) {
            const Point* pts=kContextVertices.data()+f.first;
            if(f.count==0) continue;
            if(HasBuildingShell(f)) {
                const float height=f.heightMeters>0?f.heightMeters:(f.buildingLevels>0?f.buildingLevels*3.f:6.f);
                const float base=BackgroundElevation(pts[0]);
                const bool seed=std::strcmp(f.id,"way/1180013352")==0;
                context.Extrude(pts,f.count,base,height,seed?Color{107,133,142,255}:Color{214,219,211,255},FindRoofObservation(f.id));
                const float outward=OutwardNormalSign(pts,f.count);
                // Schematic glazing, not an assertion of surveyed facade geometry.
                for(std::size_t j=1;j<f.count;++j) for(float y=2.4f;y<height-.6f;y+=3.f) {
                    const auto a=pts[j-1],b=pts[j];
                    const float len=std::hypot(b.x-a.x,b.z-a.z);
                    if(len<2.f)continue;
                    const float dx=(b.x-a.x)/len,dz=(b.z-a.z)/len;
                    for(float t=.8f;t<len-1.f;t+=seed?2.2f:3.1f) {
                        const Point p{a.x+dx*t-dz*.035f*outward,a.z+dz*t+dx*.035f*outward};
                        const Point q{p.x+dx*1.25f,p.z+dz*1.25f};
                        context.Quad(V(p,base+y-.5f),V(q,base+y-.5f),V(q,base+y+.6f),V(p,base+y+.6f),{91,120,130,255});
                    }
                }
            } else if(f.kind==ContextKind::Road || f.kind==ContextKind::Railway) {
                const bool rail=f.kind==ContextKind::Railway;
                const float width=rail?3.2f:(f.widthMeters>0?f.widthMeters:(f.lanes>0?f.lanes*3.2f:7.f));
                // layer is order only; 8m deck height is a marked visual estimate.
                const float y=rail&&f.osmLayer>0?8.f:.04f;
                for(std::size_t j=1;j<f.count;++j) {
                    context.Ribbon(pts[j-1],pts[j],width,y,rail?Color{149,155,149,255}:Color{90,96,94,255});
                    if(rail) {
                        const float dx=pts[j].x-pts[j-1].x,dz=pts[j].z-pts[j-1].z,length=std::hypot(dx,dz);
                        if(length<.01f)continue;
                        for(float offset:{-.72f,.72f}) context.Ribbon({pts[j-1].x-dz/length*offset,pts[j-1].z+dx/length*offset},{pts[j].x-dz/length*offset,pts[j].z+dx/length*offset},.12f,y+.035f,{70,79,82,255});
                        if(y>1) for(float t=0;t<length;t+=28.f) {
                            const float x=pts[j-1].x+dx/length*t,z=pts[j-1].z+dz/length*t;
                            const Point column[5]={{x-.55f,z-.55f},{x+.55f,z-.55f},{x+.55f,z+.55f},{x-.55f,z+.55f},{x-.55f,z-.55f}};
                            context.Extrude(column,5,0,y,{181,187,179,255});
                        }
                    }
                }
            }
        }
        for(const auto& f:kCourseFeatures) {
            const Point* pts=kCourseVertices.data()+f.first;
            const auto kind=[&](const char* value){return std::strcmp(f.kind,value)==0;};
            if(f.geometry==GeometryType::LineString) {
                // Observed centre references only; stripe widths are illustrative.
                // The acceleration axis is NOT an observed painted line.
                if(kind("longitudinal_painted_line_reference"))
                    for(std::size_t i=1;i<f.count;++i)course.Ribbon(pts[i-1],pts[i],.15f,.075f,{224,220,194,255});
                if(kind("crosswalk_reference")) for(std::size_t i=1;i<f.count;++i) {
                    const auto a=pts[i-1],b=pts[i];
                    const float length=std::hypot(b.x-a.x,b.z-a.z);
                    if(length<.01f)continue;
                    for(float start=0;start<length;start+=.9f) {
                        const float end=std::min(start+.45f,length);
                        course.Ribbon({a.x+(b.x-a.x)*start/length,a.z+(b.z-a.z)*start/length},
                                      {a.x+(b.x-a.x)*end/length,a.z+(b.z-a.z)*end/length},2.2f,.08f,{223,220,206,255});
                    }
                }
                continue;
            }
            if(f.geometry!=GeometryType::Polygon)continue;
            // Connector polygons overlap the outer pavement; drawing them again
            // would z-fight or paint over the separately observed islands.
            if(kind("intersection_pavement") || kind("connecting_pavement"))continue;
            if(kind("slope_plan_marking_not_elevation")) {
                for(std::size_t i=1;i<f.count;++i)course.Ribbon(pts[i-1],pts[i],.16f,.075f,{225,218,195,255});
                continue; // Actual ramp height is not known: never invent its profile.
            }
            const bool island=std::strcmp(f.kind,"non_drivable_island")==0;
            const bool parking=std::strcmp(f.kind,"parking_complex_envelope")==0;
            const bool access=kind("parking_access_stall_footprint_observed");
            const bool excluded=kind("hatched_non_driving_pavement_observed");
            const bool staging=kind("vehicle_staging_candidate");
            const float y=island?.12f:parking?.045f:access?.06f:excluded?.07f:staging?.052f:.026f;
            const Color color=island?Color{101,131,83,255}:parking?Color{121,126,88,255}:excluded?Color{155,157,135,255}:staging?Color{104,109,102,255}:Color{71,77,77,255};
            course.Polygon(pts,f.count,y,color);
            if(island || access || excluded || kind("outer_paved_envelope"))
                for(std::size_t i=1;i<f.count;++i)course.Ribbon(pts[i-1],pts[i],.18f,y+.012f,{224,220,194,255});
        }
        terrain_=terrain.Upload();context_=context.Upload();course_=course.Upload();ready_=true;
    }
    void Draw(bool terrain=true) const {
        if(!ready_)return;
        rlDisableBackfaceCulling();
        if(terrain)DrawModel(terrain_,{0,0,0},1,WHITE);
        DrawModel(context_,{0,0,0},1,WHITE);DrawModel(course_,{0,0,0},1,WHITE);
        using namespace dobong_source;
        // Real power-line ground alignments; heights and sag are explicitly estimates.
        for(const auto& f:kContextFeatures) {
            const Point* pts=kContextVertices.data()+f.first;
            if(f.kind==ContextKind::PowerTower && f.count) {
                const auto p=pts[0];
                for(float s:{-1.f,1.f}) {
                    DrawCylinderEx({p.x+s*2.4f,0,p.z-2.4f},{p.x+s*.9f,28,p.z-.9f},.16f,.08f,5,{107,117,114,255});
                    DrawCylinderEx({p.x+s*2.4f,0,p.z+2.4f},{p.x+s*.9f,28,p.z+.9f},.16f,.08f,5,{107,117,114,255});
                }
                for(float y:{12.f,20.f,27.f})DrawCylinderEx({p.x-5,y,p.z},{p.x+5,y,p.z},.16f,.16f,6,{107,117,114,255});
            }
            if(f.kind==ContextKind::PowerLine)for(std::size_t i=1;i<f.count;++i) for(float offset:{-3.f,0.f,3.f}) {
                Vector3 prev{pts[i-1].x+offset,27,pts[i-1].z};
                for(int part=1;part<=12;++part) {
                    const float t=part/12.f;
                    Vector3 next{pts[i-1].x+(pts[i].x-pts[i-1].x)*t+offset,27-5*4*t*(1-t),pts[i-1].z+(pts[i].z-pts[i-1].z)*t};
                    DrawLine3D(prev,next,{66,78,77,255});prev=next;
                }
            }
        }
        rlEnableBackfaceCulling();
    }
    void Unload() { if(ready_){UnloadModel(terrain_);UnloadModel(context_);UnloadModel(course_);ready_=false;} }
};
} // namespace dobong_visual
