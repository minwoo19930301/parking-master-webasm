// Compile the actual production member body with non-rendering dependencies.
// This tests completion/retry wiring, not the graphical app or all input paths.
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import test from 'node:test';
import assert from 'node:assert/strict';
const root=fileURLToPath(new URL('../',import.meta.url));

test('production road completion freezes time/pose and still allows explicit retry',()=>{
  const source=fs.readFileSync(path.join(root,'src/main.cpp'),'utf8');
  const start=source.indexOf('    void UpdateFreeDrive(float dt, const InputFrame& input) {');
  assert.ok(start>=0);const opening=source.indexOf('{',start);let depth=1,end=opening+1;
  for(;depth&&end<source.length;end++){if(source[end]==='{')depth++;else if(source[end]==='}')depth--;}
  assert.equal(depth,0);const method=source.slice(start,end);
  const fixture=`
#include "src/road_driving.h"
#include <cassert>
#include <string>
struct Vector2 {float x,y;};
Vector2 VSub(Vector2 a,Vector2 b){return {a.x-b.x,a.y-b.y};}
float Vector2Length(Vector2 a){return std::hypot(a.x,a.y);}
struct CarState {Vector2 position{10,0};float heading=0,speed=0;};
struct InputFrame {bool retryPressed=false;};
enum class TransmissionGear {Park,Drive};
enum class CollisionKind {None,Wall};
struct FakeTraffic {int calls=0;void Update(float,road_driving::Point,float){calls++;}};
struct Fixture {
  bool roadMode_=true,parkingBrake_=false,collide=false;int roadRoute_=0,vehicleCalls=0,retries=0,freeResets=0,speeches=0;std::size_t roadManeuver_=0;
  float examTimer_=99.5f,collisionFlash_=0;CarState car_;FakeTraffic traffic_;
  TransmissionGear gear_=TransmissionGear::Drive;road_driving::Progress roadProgress_;road_driving::Path roadPath_;
  void UpdateVehicle(float,const InputFrame&){vehicleCalls++;}
  CollisionKind DetectCollision(){return collide?CollisionKind::Wall:CollisionKind::None;}
  void BeginRoadDrive(int index){assert(index==roadRoute_);retries++;examTimer_=0;roadProgress_={};}
  void BeginFreeDrive(){freeResets++;}
  void Speak(const std::string&){speeches++;}
${method}
};
int main(){
  Fixture done;done.roadProgress_.complete=true;done.car_.speed=5;
  done.UpdateFreeDrive(.05f,{});
  assert(done.examTimer_==99.5f&&done.vehicleCalls==0&&done.car_.position.x==10);
  assert(done.car_.speed==0&&done.parkingBrake_&&done.gear_==TransmissionGear::Park);
  assert(done.traffic_.calls==1&&done.speeches==0);
  done.UpdateFreeDrive(.05f,{true});assert(done.retries==1&&!done.roadProgress_.complete&&done.examTimer_==0);
  Fixture finish;const road_driving::Point points[]={{0,0},{10,0}};finish.roadPath_=road_driving::Path(points,2);finish.roadProgress_.distance=9;
  finish.UpdateFreeDrive(.05f,{});assert(finish.roadProgress_.complete&&finish.vehicleCalls==1&&finish.speeches==1);
  const float finalTime=finish.examTimer_;assert(finalTime>99.5f);
  for(int i=0;i<120;i++)finish.UpdateFreeDrive(.05f,{});
  assert(finish.examTimer_==finalTime&&finish.vehicleCalls==1&&finish.speeches==1&&finish.traffic_.calls==121);
  Fixture free;free.roadMode_=false;free.UpdateFreeDrive(.05f,{});assert(free.vehicleCalls==1&&free.examTimer_==99.5f);
  free.UpdateFreeDrive(.05f,{true});assert(free.freeResets==1);
}
`;
  const directory=fs.mkdtempSync(path.join(root,'build-test/road-completion-'));
  const cpp=path.join(directory,'fixture.cpp'),binary=path.join(directory,'fixture');
  fs.writeFileSync(cpp,fixture);
  const compile=spawnSync('c++',['-std=c++17','-Wall','-Wextra','-Werror','-I',root,cpp,'-o',binary],{encoding:'utf8'});
  assert.equal(compile.status,0,compile.stderr);
  const run=spawnSync(binary,[],{encoding:'utf8'});assert.equal(run.status,0,run.stderr);
});
