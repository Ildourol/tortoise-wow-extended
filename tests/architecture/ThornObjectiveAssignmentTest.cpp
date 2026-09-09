#include "ThornGorgeRules.h"
#include <map>
#include <iostream>
#include <stdexcept>
using uint32=unsigned;
constexpr unsigned STATUS_IN_PROGRESS=3;
constexpr unsigned NodeLocations[4]={0,1,2,3};
struct Point {float x,y,z;};
struct Locations {Point points[4]={{0,0,0},{100,0,0},{100,100,0},{0,100,0}};Point const* LookupEntry(unsigned i){return i<4?&points[i]:nullptr;}}sWorldSafeLocsStore;
struct Player {
 unsigned guid=3;ThornGorge::Team team=ThornGorge::Alliance;bool eligible=true;float x=0,y=0,z=0;
 unsigned GetObjectGuid(){return guid;}unsigned GetGUIDLow(){return guid;}
 float GetPositionX(){return x;}float GetPositionY(){return y;}float GetPositionZ(){return z;}
 float GetDistanceSqr(float a,float b,float c){return (x-a)*(x-a)+(y-b)*(y-b)+(z-c)*(z-c);}
};
struct GameObject {bool spawned=true;bool isSpawned(){return spawned;}float GetPositionX(){return 50;}float GetPositionY(){return 50;}float GetPositionZ(){return 0;}};
struct Map {std::map<unsigned,Player*> players;GameObject flag;Player* GetPlayer(unsigned i){auto it=players.find(i);return it==players.end()?nullptr:it->second;}GameObject* GetGameObject(unsigned g){return g?&flag:nullptr;}};
struct BattleGroundTG {
 unsigned status=3,m_carrier=0,availableFlag=1;ThornGorge::Rules m_rules;mutable Map map;
 unsigned GetStatus()const{return status;}bool Eligible(Player* p)const{return p&&p->eligible;}
 ThornGorge::Team Side(Player* p)const{return p->team;}Map* GetBgMap()const{return &map;}
 unsigned GetAvailableFlag()const{return availableFlag;}
 bool GetObjective(Player*,float&,float&,float&)const;
};
#include "ThornObjectiveNative.inc"
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
int main(){
 BattleGroundTG bg;Player p;float x,y,z;
 // Capture assignments spread across neutral bases before a team owns any.
 for(unsigned id=3;id<6;++id){p.guid=id;Check(bg.GetObjective(&p,x,y,z),"capture assignment missing");auto n=sWorldSafeLocsStore.points[id%4];Check(x==n.x&&y==n.y,"capture assignments collapsed");}
 p.guid=6;bg.m_rules.owner[0]=ThornGorge::Alliance;Check(bg.GetObjective(&p,x,y,z)&&x==50&&y==50,"runner ignores available flag");
 bg.map.flag.spawned=false;Check(bg.GetObjective(&p,x,y,z)&&x==100,"runner targeted absent flag");bg.map.flag.spawned=true;
 p.guid=8;Check(bg.GetObjective(&p,x,y,z)&&x==0&&y==0,"defender abandoned owned base");
 Player carrier;carrier.guid=99;carrier.x=75;carrier.y=40;bg.m_carrier=99;bg.map.players[99]=&carrier;p.guid=7;
 Check(bg.GetObjective(&p,x,y,z)&&x==75&&y==40,"escort assignment missing");carrier.team=ThornGorge::Horde;
 Check(bg.GetObjective(&p,x,y,z)&&x==75,"intercept assignment missing");carrier.eligible=false;
 Check(bg.GetObjective(&p,x,y,z)&&!(x==75&&y==40),"invalid carrier retained");
 p.guid=99;p.x=95;p.y=5;bg.m_rules.owner[1]=ThornGorge::Alliance;
 Check(bg.GetObjective(&p,x,y,z)&&x==100&&y==0,"carrier ignored nearest owned base");
 bg.m_rules.owner.fill(ThornGorge::Neutral);Check(bg.GetObjective(&p,x,y,z)&&x==100&&y==0,"carrier has no objective with no owned base");
 bg.m_rules.owner.fill(ThornGorge::Alliance);bg.m_carrier=0;bg.availableFlag=0;p.guid=3;
 Check(bg.GetObjective(&p,x,y,z),"all bases owned strands noncarrier");
 p.eligible=false;Check(!bg.GetObjective(&p,x,y,z),"ineligible player assigned");p.eligible=true;bg.status=4;
 Check(!bg.GetObjective(&p,x,y,z),"finished match assigns objective");
 std::cout<<"Native TG runner, capture, escort/intercept, defence and carrier fallback assignments passed\n";
}
