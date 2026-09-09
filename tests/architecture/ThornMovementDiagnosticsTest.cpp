#include <vector>
#include <sstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <limits>
using uint32=unsigned;
constexpr float INVALID_HEIGHT=-100000;
constexpr unsigned LOG_BG=1,MOVE_RUN=1,MOVE_SWIM=2;
enum AuraType {SPELL_AURA_MOD_INCREASE_SPEED=31,SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED=32,SPELL_AURA_MOD_SPEED_ALWAYS=129,SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS=130,SPELL_AURA_MOD_SPEED_NOT_STACK=171,SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK=172,SPELL_AURA_MOD_DECREASE_SPEED=33};
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
struct Vec{float x=1,y=2,z=3;};
struct Spline {
 bool initialized=false; int index=2;std::vector<Vec> points=std::vector<Vec>(50);
 bool Initialized(){return initialized;}
 auto const& getPath(){Check(initialized,"fresh spline points");return points;}
 int _currentSplineIdx(){return index;}
 unsigned GetId(){return 8;} unsigned GetFlags(){return 16;} unsigned GetTransportGuid(){return 23;}
 bool Finalized(){Check(initialized,"fresh spline state");return false;}
 int timePassed(){return 100;} int Duration(){return 5000;}
};
struct Aura{struct Mod{int m_amount=100;}mod;unsigned GetId(){return 123;}Mod* GetModifier(){return &mod;}};
struct Map{float floor=90;unsigned queries=0;float GetHeight(float,float,float z,bool v,float dist){++queries;Check(z==100.5f&&v&&dist==100,"wrong floor ray");return floor;}};
struct Motion{unsigned GetCurrentMovementGeneratorType(){return 8;}};
struct Player {
 bool world=true,teleport=false;Map* map=nullptr;Spline* movespline=nullptr;Motion motion;
 std::vector<Aura*> auras;
 struct Info{unsigned GetMovementFlags(){return 4096;}}m_movementInfo;
 bool IsInWorld(){return world;}Map* GetMap(){return map;}bool IsBeingTeleported(){return teleport;}
 float GetPositionX(){return 1;}float GetPositionY(){return 2;}float GetPositionZ(){return 100;}
 unsigned GetGUIDLow(){return 7;}unsigned GetUnitState(){return 4;}
 Motion* GetMotionMaster(){return &motion;}float GetSpeed(unsigned){return 14;}bool IsMounted(){return false;}
 auto const& GetAurasByType(AuraType){return auras;}
};
struct Log {std::string line;unsigned count=0;template<class...A>void out(unsigned,char const* fmt,A...a){char b[4096];int n=std::snprintf(b,sizeof b,fmt,a...);Check(n>0&&n<sizeof b,"format length");line=b;++count;}}sLog;
struct BattleGroundTG {
 struct Budget{unsigned level=0;}m_diagnostics;
 Map map;unsigned m_elapsed=5000;
 Map* GetBgMap(){return &map;}unsigned GetInstanceID(){return 101;}
 void TraceMovement(Player*,unsigned);
};
#include "ThornMovementNative.inc"
int main(){
 BattleGroundTG bg;Player p;p.map=&bg.map;Spline spline;p.movespline=&spline;
 bg.TraceMovement(&p,3);Check(!sLog.count&&!bg.map.queries,"disabled trace performed work");
 bg.m_diagnostics.level=1;bg.TraceMovement(&p,3);Check(!sLog.count,"level1 movement");
 bg.m_diagnostics.level=2;p.teleport=true;bg.TraceMovement(&p,3);Check(!sLog.count,"teleport sample");p.teleport=false;
 p.map=nullptr;bg.TraceMovement(&p,3);Check(!sLog.count,"foreign map sample");p.map=&bg.map;
 bg.TraceMovement(&p,3);Check(sLog.line.find("floor_valid=1 floor_z=90.000 floor_gap=10.000")!=std::string::npos,"floor evidence missing");
 Check(sLog.line.find("spline_initialized=0")!=std::string::npos,"fresh spline guard");
 spline.initialized=true;Aura aura;p.auras=std::vector<Aura*>(50,&aura);bg.TraceMovement(&p,4);
 Check(sLog.line.find("spline_points=8 vertices=2:1,2,3;3:")!=std::string::npos,"bounded vertices wrong");
 Check(sLog.line.find(";10:")==std::string::npos,"vertex cap exceeded");
 auto a=sLog.line.substr(sLog.line.find("speed_auras="));Check(std::count(a.begin(),a.end(),';')==7,"aura cap exceeded");
 Check(sLog.line.find("spline_transport=23")!=std::string::npos,"transport context missing");
 for(float f:{INVALID_HEIGHT,std::numeric_limits<float>::quiet_NaN()}){bg.map.floor=f;bg.TraceMovement(&p,5);Check(sLog.line.find("floor_valid=0 floor_z=0.000 floor_gap=0.000")!=std::string::npos,"invalid floor fabricated");}
 spline.index=100;bg.TraceMovement(&p,6);Check(sLog.line.find("spline_points=0 vertices=-")!=std::string::npos,"out of range vertex read");
 p.movespline=nullptr;bg.TraceMovement(&p,7);Check(sLog.line.find("spline_initialized=0")!=std::string::npos,"null spline");
 std::cout<<"Native bounded movement observations and lifecycle guards passed\n";
}
