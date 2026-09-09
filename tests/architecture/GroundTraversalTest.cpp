#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>
#include <iostream>
using uint32=unsigned;constexpr unsigned BATTLEGROUND_TG=6,STATUS_IN_PROGRESS=3,MOVE_RUN=0,MOVE_WALK=1,LOG_BG=0,MOVEFLAG_SWIMMING=1,MOVEFLAG_FALLING=2;
constexpr float M_PI_F=3.14159265f;
unsigned clockNow=10000;struct WorldTimer{static unsigned getMSTime(){return clockNow;}static unsigned getMSTimeDiff(unsigned a,unsigned b){return b-a;}};
struct BattleGround{unsigned type=6,status=3;unsigned GetTypeId(){return type;}unsigned GetStatus(){return status;}};
struct BattleGroundTG:BattleGround{bool AdmitBotDiagnostic(void*){return false;}};
struct Player{BattleGroundTG* bg;bool dead=false,casting=false,flying=false,water=false,transport=false;unsigned flags=0,map=821;unsigned GetMapId(){return map;}bool IsFlying(){return flying;}bool IsFalling(){return (flags&MOVEFLAG_FALLING)!=0;}bool IsInWater(){return water;}bool HasMovementFlag(unsigned f){return (flags&f)!=0;}unsigned GetInstanceId(){return 7;}unsigned GetGUIDLow(){return 1;}BattleGround* GetBattleGround(){return bg;}bool IsInWorld(){return true;}bool IsDead(){return dead;}bool IsNonMeleeSpellCasted(bool){return casting;}bool GetTransport(){return transport;}float GetSpeed(unsigned t){return t?2.5f:7.f;}};
struct AI{bool movable=true,jumping=false;bool CanMove(){return movable;}bool IsJumping(){return jumping;}};
bool hasWalk=false,landingGround=true,progress=true,validArc=true,canLand=true,goodArc=true;
unsigned probes=0,moves=0;float usedSpeed=0,usedVertical=0;
struct WorldPosition{
 unsigned map=821;float x=0,z=0;bool valid=true;
 WorldPosition(){}WorldPosition(Player* p):map(p->map){}WorldPosition(float px,float pz=0):x(px),z(pz){}
 unsigned getMapId()const{return map;}float getX()const{return x;}float getY()const{return 0;}float getZ()const{return z;}
 float distance(WorldPosition const& b)const{return std::hypot(x-b.x,z-b.z);}float getAngleTo(WorldPosition const&)const{return 0;}
 operator bool()const{return valid;}
 bool ClosestCorrectPoint(float,float,unsigned){return landingGround;}
 std::vector<WorldPosition> getPathStepFrom(WorldPosition const& src,Player*,bool)const{
  if(src.x==0)return hasWalk?std::vector<WorldPosition>{src,WorldPosition(10)}:std::vector<WorldPosition>{};
  return {src,WorldPosition(progress?90:1)};
 }
};
struct Config{float jumpVSpeed=20;}sPlayerbotAIConfig;
struct Log{static Log& Instance(){static Log l;return l;}template<class...T>void out(T...){}};
struct JumpAction{
 Player* bot;AI* ai;unsigned m_lastTraversalAttempt=0;bool TryGroundTraversal(WorldPosition const&);
 static WorldPosition CalculateJumpParameters(WorldPosition const&,Player*,float,float vs,float hs,float& t,float& d,float& h,bool& good,std::vector<WorldPosition>& arc){++probes;usedSpeed=hs;usedVertical=vs;t=1;d=7;h=1.6f;good=goodArc;arc={WorldPosition(3,1.6f),WorldPosition(7)};WorldPosition p(7);p.valid=validArc;return p;}
 static bool CanLand(WorldPosition const&,Player*){return canLand;}
 bool DoJump(WorldPosition const&,WorldPosition const&,float,float,float,float,float,float,bool,bool,bool,bool){++moves;return true;}
};
#define MANGOSBOT_ZERO
#include "GroundTraversalNative.inc"
void Check(bool b,char const*m){if(!b)throw std::runtime_error(m);}
int main(){BattleGroundTG bg;Player p{&bg};AI ai;JumpAction jump{&p,&ai};WorldPosition goal(100);
 Check(jump.TryGroundTraversal(goal)&&moves==1,"safe progressive native jump rejected");Check(usedSpeed==7&&usedVertical==7.96f,"player physics bounds lost");
 Check(!jump.TryGroundTraversal(goal)&&probes==1,"traversal throttle failed");clockNow+=5000;
 hasWalk=true;Check(!jump.TryGroundTraversal(goal)&&probes==1,"jump replaced available walking");hasWalk=false;
 for(bool* guard:{&validArc,&goodArc,&canLand,&landingGround,&progress}){clockNow+=5000;*guard=false;unsigned before=moves,start=probes;Check(!jump.TryGroundTraversal(goal)&&moves==before,"unverified landing caused movement");Check(probes-start<=16,"unbounded traversal search");*guard=true;}
 for(unsigned map:{0u,1u,30u,489u,529u,33u,409u,821u}){p.map=map;goal.map=map;p.bg=nullptr;clockNow+=5000;unsigned before=moves;Check(jump.TryGroundTraversal(goal)&&moves==before+1,"generic map traversal rejected");}
 p.bg=&bg;bg.status=0;clockNow+=5000;Check(!jump.TryGroundTraversal(goal),"jump during preparation");bg.status=3;
 for(bool* guard:{&p.flying,&p.water,&p.transport,&p.dead}){*guard=true;clockNow+=5000;unsigned before=probes;Check(!jump.TryGroundTraversal(goal)&&probes==before,"invalid ground state probed");*guard=false;}
 for(unsigned flag:{MOVEFLAG_SWIMMING,MOVEFLAG_FALLING}){p.flags=flag;Check(!jump.TryGroundTraversal(goal),"air/water state probed");}p.flags=0;
 goal.map=1;Check(!jump.TryGroundTraversal(goal),"cross-map jump");goal.map=p.map;
 ai.jumping=true;Check(!jump.TryGroundTraversal(goal),"overlapping jump");ai.jumping=false;ai.movable=false;Check(!jump.TryGroundTraversal(goal),"rooted movement");ai.movable=true;p.casting=true;Check(!jump.TryGroundTraversal(goal),"cast interruption");
 std::cout<<"Native generic ground traversal eligibility, bounded search, physics limits and safe-progress gates passed\n";
}
