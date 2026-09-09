#include "ThornGorgeRules.h"
#include <string>
#include <iostream>
#include <stdexcept>
using uint32=unsigned;
constexpr unsigned STATUS_WAIT_LEAVE=4;
struct BattleGroundTG;
struct Session{bool socket=true;void* GetSocket(){return socket?this:nullptr;}};
struct Player{
 bool world=true;BattleGroundTG* bg=nullptr;Session* session=nullptr;ThornGorge::Team team=ThornGorge::Horde;
 std::string prefix,payload;unsigned sends=0;
 bool IsInWorld(){return world;}BattleGroundTG* GetBattleGround(){return bg;}Session* GetSession(){return session;}
 void SendAddonMessage(std::string p,std::string m){prefix=p;payload=m;++sends;}
};
struct Map{Player* carrier=nullptr;Player* GetPlayer(unsigned){return carrier;}};
struct BattleGroundTG{
 ThornGorge::Rules m_rules;unsigned m_carrier=0,status=3;Map map;
 unsigned GetInstanceID(){return 101;}unsigned GetStatus(){return status;}Map* GetBgMap(){return &map;}
 ThornGorge::Team Side(Player* p){return p->team;}void SendClientState(Player*);
};
#include "ThornClientStateNative.inc"
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
int main(){
 BattleGroundTG bg;Player p;Session s;p.bg=&bg;p.session=&s;
 bg.SendClientState(&p);Check(p.prefix=="MT_TG1"&&p.payload=="1;101;3;0;2;0","initial neutral state wrong");
 bg.m_rules.flag=ThornGorge::Carried;bg.m_carrier=1;bg.map.carrier=&p;
 bg.SendClientState(&p);Check(p.payload=="1;101;3;1;1;0","Horde carrier team missing");p.team=ThornGorge::Alliance;
 bg.SendClientState(&p);Check(p.payload=="1;101;3;1;0;0","Alliance carrier team missing");
 bg.m_carrier=0;bg.m_rules.flag=ThornGorge::Respawning;bg.m_rules.flagTimer=9980;
 bg.SendClientState(&p);Check(p.payload=="1;101;3;3;2;9980","authoritative reset timer changed");
 bg.m_rules.ended=true;bg.SendClientState(&p);Check(p.payload=="1;101;4;3;2;9980","finished status stale");
 unsigned before=p.sends;s.socket=false;bg.SendClientState(&p);s.socket=true;p.world=false;bg.SendClientState(&p);
 p.world=true;p.bg=nullptr;bg.SendClientState(&p);p.bg=&bg;p.session=nullptr;bg.SendClientState(&p);bg.SendClientState(nullptr);
 Check(before==p.sends,"UI state sent to synthetic/foreign/offline recipient");
 std::cout<<"Native display-only TG client state and recipient guards passed\n";
}
