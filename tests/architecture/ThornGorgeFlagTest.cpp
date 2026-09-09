// Compile the actual native flag functions against a small map/player harness.
// This exercises reentrant aura removal and failure paths without a live DB.
#include "ThornGorgeRules.h"
#include <iostream>
#include <stdexcept>
#include <array>
using uint32=unsigned;
struct ObjectGuid {
    unsigned long long value=0;
    ObjectGuid(unsigned long long v=0):value(v){}
    explicit operator bool()const{return value!=0;}
    void Clear(){value=0;}
    unsigned GetCounter()const{return unsigned(value);}
    bool operator==(ObjectGuid b)const{return value==b.value;}
    bool operator!=(ObjectGuid b)const{return value!=b.value;}
};
constexpr unsigned CenterObject=12,DroppedObject=13,CarrySpell=59005,PickupSpell=59011;
constexpr unsigned STATUS_IN_PROGRESS=3,RESPAWN_NEVER=86400,RESPAWN_IMMEDIATELY=0;
struct GameObject {
    ObjectGuid guid; void* map=nullptr; bool spawned=true; float scale=1;
    void SetObjectScale(float value){scale=value;}
    void* GetMap(){return map;} bool isSpawned(){return spawned;}
    ObjectGuid GetObjectGuid(){return guid;}
};
class BattleGroundTG;
struct Player {
    BattleGroundTG* bg=nullptr; void* map=nullptr; ObjectGuid guid=11;
    bool eligible=true,canUse=true,range=true,los=true,aura=false,castSucceeds=true;
    unsigned casts=0,removals=0;
    void* GetMap(){return map;} ObjectGuid GetObjectGuid(){return guid;}
    unsigned long long GetGUID(){return guid.value;}
    bool CanUseBattleGroundObject(){return canUse;}
    bool IsWithinDistInMap(GameObject*,float){return range;}
    bool IsWithinLOSInMap(GameObject*){return los;}
    void CastSpell(Player*,unsigned spell,bool triggered,void* =nullptr,void* =nullptr,ObjectGuid ={})
    { ++casts; if(spell==CarrySpell && triggered && castSucceeds) aura=true; }
    bool HasAura(unsigned){return aura;}
    void RemoveAurasDueToSpell(unsigned);
    float GetPositionX(){return 1;} float GetPositionY(){return 2;} float GetPositionZ(){return 3;}
};
class BattleGroundTG {
public:
    ThornGorge::Rules m_rules;
    ObjectGuid m_carrier;
    std::array<ObjectGuid,14> m_BgObjects{};
    GameObject center,drop; unsigned status=3,adds=0; bool addSucceeds=true;
    BattleGroundTG(){m_BgObjects[12]=1;center.guid=1;center.map=this;drop.map=this;}
    BattleGroundTG* GetBgMap()const{return const_cast<BattleGroundTG*>(this);}
    GameObject* GetGameObject(ObjectGuid g){return g==drop.guid?&drop:nullptr;}
    float m_flagScale=2.5f;
    unsigned GetStatus(){return status;}
    bool Eligible(Player* p){return p&&p->eligible&&p->map==GetBgMap();}
    void Announce(char const*){} void SendStates(){}
    unsigned sound=0;void PlaySoundToAll(unsigned id){sound=id;}
    void SpawnObject(ObjectGuid g,unsigned respawn){(g==center.guid?center:drop).spawned=respawn==0;}
    void DelObject(unsigned slot){m_BgObjects[slot].Clear();drop.spawned=false;}
    bool AddObject(unsigned slot,unsigned,float,float,float,int,int,int,int,int,float scale)
    {++adds;if(!addSucceeds)return false;drop.guid=100+adds;m_BgObjects[slot]=drop.guid;drop.spawned=true;drop.scale=scale;return true;}
    void Trace(char const*,Player* =nullptr,unsigned =0,char const* ="-",bool =false){}
    char const* FlagRejection(Player*,GameObject*);
    bool OwnFlagObject(GameObject*)const;
    void EventPlayerClickedOnFlag(Player*,GameObject*);
    void CompleteFlagPickup(Player*,GameObject*);
    void EventPlayerDroppedFlag(Player*);
    void RestoreFlag();
};
void Player::RemoveAurasDueToSpell(unsigned)
{ ++removals; if(aura){aura=false;bg->EventPlayerDroppedFlag(this);} }
#include "ThornGorgeFlagNative.inc"
static void Check(bool ok,char const* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    Check(ThornGorge::FlagCountdownSeconds(6000,5000)==5,"five-second countdown");
    Check(ThornGorge::FlagCountdownSeconds(5000,4999)==0,"no duplicate countdown");
    Check(ThornGorge::FlagCountdownSeconds(8000,1999)==2,"stall emits only current time");
    Check(ThornGorge::FlagCountdownSeconds(1,0)==0,"no zero after reset");
    BattleGroundTG bg; Player p; p.bg=&bg;p.map=&bg;
    bg.EventPlayerClickedOnFlag(&p,&bg.center);
    Check(p.casts==1 && !bg.m_carrier,"click starts cast; cannot grant flag early");
    p.range=false;bg.CompleteFlagPickup(&p,&bg.center);
    Check(!bg.m_carrier,"moving out of range during cast rejects pickup");
    p.range=true;p.canUse=false;bg.CompleteFlagPickup(&p,&bg.center);
    Check(!bg.m_carrier,"mounted/immune completion rejected");
    p.canUse=true;p.los=false;bg.CompleteFlagPickup(&p,&bg.center);
    Check(!bg.m_carrier,"LOS lost during cast rejects pickup");
    p.los=true;GameObject foreign=bg.center;foreign.map=nullptr;
    bg.CompleteFlagPickup(&p,&foreign);Check(!bg.m_carrier,"cross-map object rejected");
    bg.CompleteFlagPickup(&p,&bg.center);
    Check(bg.m_carrier==p.guid && p.aura && !bg.center.spawned,"successful native pickup");
    Player other=p;other.guid=22;other.aura=false;
    bg.CompleteFlagPickup(&other,&bg.center);bg.EventPlayerDroppedFlag(&other);
    Check(bg.m_carrier==p.guid && bg.adds==0,"stale callbacks preserve current carrier");
    bg.EventPlayerDroppedFlag(&p);
    Check(bg.adds==1 && !bg.m_carrier && !p.aura && bg.drop.spawned,"reentrant aura callback makes exactly one drop");
    Check(bg.drop.scale==2.5f,"dropped flag scale lost");
    bg.m_rules.Tick(2107);
    bg.EventPlayerClickedOnFlag(&other,&bg.drop);
    bg.m_rules.Tick(10000);
    bg.CompleteFlagPickup(&other,&bg.drop);
    Check(bg.m_carrier==other.guid && other.aura,"dropped flag pickup");
    bg.addSucceeds=false;bg.EventPlayerDroppedFlag(&other);
    Check(bg.m_rules.flag==ThornGorge::Respawning && !bg.m_carrier && !other.aura,"spawn failure clears carrier and schedules reset");
    Check(bg.m_rules.flagTimer==10000,"failed ground spawn retains ten-second center reset");
    bg.m_rules.Tick(10000);bg.RestoreFlag();
    Check(bg.sound==8232,"native WSG reset sound missing");
    Check(bg.center.spawned && !bg.m_BgObjects[13],"native reset restores center and removes dropped object");
    p.castSucceeds=false;bg.addSucceeds=true;bg.CompleteFlagPickup(&p,&bg.center);
    Check(!bg.m_carrier && bg.m_rules.flag==ThornGorge::Dropped,"carry aura failure never strands carrier state");
    bg.m_rules.Tick(10000);bg.RestoreFlag();bg.status=4;
    bg.CompleteFlagPickup(&p,&bg.center);Check(!bg.m_carrier,"completed match rejects delayed pickup");
    std::cout<<"Native Thorn Gorge flag callbacks passed\n";
}
