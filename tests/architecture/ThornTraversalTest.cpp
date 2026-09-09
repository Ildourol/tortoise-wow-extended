#include <algorithm>
#include <cmath>
#include <vector>
#include <stdexcept>
#include <iostream>
using uint32=unsigned;constexpr unsigned BATTLEGROUND_TG=6,STATUS_IN_PROGRESS=3,MOVE_RUN=0,MOVE_WALK=1,LOG_BG=0;
constexpr float M_PI_F=3.14159265f;
unsigned clockNow=10000;struct WorldTimer{static unsigned getMSTime(){return clockNow;}static unsigned getMSTimeDiff(unsigned a,unsigned b){return b-a;}};
struct BattleGround{unsigned type=6,status=3;unsigned GetTypeId(){return type;}unsigned GetStatus(){return status;}};
struct BattleGroundTG:BattleGround{bool AdmitBotDiagnostic(void*){return false;}};
struct Player{BattleGroundTG* bg;bool dead=false,casting=false;unsigned GetMapId(){return 821;}unsigned GetInstanceId(){return 7;}unsigned GetGUIDLow(){return 1;}BattleGround* GetBattleGround(){return bg;}bool IsInWorld(){return true;}bool IsDead(){return dead;}bool IsNonMeleeSpellCasted(bool){return casting;}bool GetTransport(){return false;}float GetSpeed(unsigned t){return t?2.5f:7.f;}};
struct AI{bool movable=true,jumping=false;bool CanMove(){return movable;}bool IsJumping(){return jumping;}};
bool hasWalk=false,landingGround=true,progress=true,validArc=true,canLand=true,goodArc=true;
unsigned probes=0,moves=0;float usedSpeed=0,usedVertical=0;
struct WorldPosition{
 unsigned map=821;float x=0,z=0;bool valid=true;
 WorldPosition(){}WorldPosition(Player*){}WorldPosition(float px,float pz=0):x(px),z(pz){}
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
 Player* bot;AI* ai;unsigned m_lastTraversalAttempt=0;bool TryThornTraversal(WorldPosition const&);
 static WorldPosition CalculateJumpParameters(WorldPosition const&,Player*,float,float vs,float hs,float& t,float& d,float& h,bool& good,std::vector<WorldPosition>& arc){++probes;usedSpeed=hs;usedVertical=vs;t=1;d=7;h=1.6f;good=goodArc;arc={WorldPosition(3,1.6f),WorldPosition(7)};WorldPosition p(7);p.valid=validArc;return p;}
 static bool CanLand(WorldPosition const&,Player*){return canLand;}
 bool DoJump(WorldPosition const&,WorldPosition const&,float,float,float,float,float,float,bool,bool,bool,bool){++moves;return true;}
};
#define MANGOSBOT_ZERO
#include "ThornTraversalNative.inc"
void Check(bool b,char const*m){if(!b)throw std::runtime_error(m);}
int main(){BattleGroundTG bg;Player p{&bg};AI ai;JumpAction jump{&p,&ai};WorldPosition goal(100);
 Check(jump.TryThornTraversal(goal)&&moves==1,"safe progressive native jump rejected");Check(usedSpeed==7&&usedVertical==7.96f,"player physics bounds lost");
 Check(!jump.TryThornTraversal(goal)&&probes==1,"traversal throttle failed");clockNow+=5000;
 hasWalk=true;Check(!jump.TryThornTraversal(goal)&&probes==1,"jump replaced available walking");hasWalk=false;
 for(bool* guard:{&validArc,&goodArc,&canLand,&landingGround,&progress}){clockNow+=5000;*guard=false;unsigned before=moves,start=probes;Check(!jump.TryThornTraversal(goal)&&moves==before,"unverified landing caused movement");Check(probes-start<=16,"unbounded traversal search");*guard=true;}
 for(unsigned type:{0u,1u,2u,3u,4u,5u}){bg.type=type;clockNow+=5000;unsigned before=probes;Check(!jump.TryThornTraversal(goal)&&probes==before,"TG traversal leaked to other map");}bg.type=6;
 ai.jumping=true;Check(!jump.TryThornTraversal(goal),"overlapping jump");ai.jumping=false;ai.movable=false;Check(!jump.TryThornTraversal(goal),"rooted movement");ai.movable=true;p.casting=true;Check(!jump.TryThornTraversal(goal),"cast interruption");
 std::cout<<"Native TG traversal eligibility, bounded search, physics limits and safe-progress gates passed\n";
}
