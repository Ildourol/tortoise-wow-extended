#include <stdexcept>
#include <iostream>
#include <list>
#include <string>
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
constexpr unsigned BATTLEGROUND_TG=6,STATUS_IN_PROGRESS=3;
constexpr float ACTION_EMERGENCY=90;
struct BattleGround{unsigned type=6,status=3,carrier=7;unsigned GetTypeId(){return type;}unsigned GetStatus(){return status;}unsigned GetFlagCarrierGuid(){return carrier;}};
struct Player{BattleGround* bg=nullptr;Player* victim=nullptr;Player* enemy=nullptr;bool aura=true,casting=false,near=false,combat=false;float distance=100,victimDistance=5;unsigned GetObjectGuid(){return 7;}BattleGround* GetBattleGround(){return bg;}bool HasAura(unsigned){return aura;}bool IsNonMeleeSpellCasted(bool){return casting;}bool IsWithinDist3d(float,float,float,float r){return near||distance<=r;}Player* GetVictim(){return victim;}bool IsInCombat(){return combat;}bool IsWithinDistInMap(Player*,float r){return victimDistance<=r;}};
using Unit=Player;
struct BattleGroundTG:BattleGround {bool eligible=true;bool GetObjective(Player*,float&,float&,float&){return eligible;}};
struct ThornFlagDelivery{Player* bot;bool IsActive();};
struct ThornObjectiveTravel{Player* bot;bool IsActive();};
struct ThornCarrierIntercept{Player* bot;bool IsActive();};
#define AI_VALUE(type, name) bot->enemy
#include "ThornCarrierTriggerNative.inc"
struct NextAction{std::string name;float priority;NextAction(char const* n,float p):name(n),priority(p){}static NextAction* array(int,NextAction* a,void*){return a;}};
struct TriggerNode{std::string name;NextAction* action;TriggerNode(char const* n,NextAction* a):name(n),action(a){}~TriggerNode(){delete action;}};
struct ThornGorgeStrategy{void InitNonCombatTriggers(std::list<TriggerNode*>&);void InitCombatTriggers(std::list<TriggerNode*>&);};
#include "ThornCarrierStrategyNative.inc"
int main(){
 BattleGroundTG bg;Player p;p.bg=&bg;ThornFlagDelivery t{&p};Check(t.IsActive(),"carrier should deliver");
 for(unsigned type:{0u,1u,2u,3u,4u,5u}){bg.type=type;Check(!t.IsActive(),"other BG behavior changed");}bg.type=6;
 bg.carrier=99;Check(!t.IsActive(),"noncarrier priority");bg.carrier=7;
 bg.eligible=false;Check(!t.IsActive(),"no owned/eligible base");bg.eligible=true;
 p.near=true;Check(!t.IsActive(),"arrival movement priority");p.near=false;
 p.casting=true;Check(!t.IsActive(),"cast interruption");p.casting=false;
 p.aura=false;Check(!t.IsActive(),"missing carry aura");p.aura=true;
 bg.status=4;Check(!t.IsActive(),"ended BG");bg.status=3;
 p.bg=nullptr;Check(!t.IsActive(),"world behavior changed");
 ThornGorgeStrategy strategy;
 for(bool combat:{false,true}){std::list<TriggerNode*> nodes;if(combat)strategy.InitCombatTriggers(nodes);else strategy.InitNonCombatTriggers(nodes);
 Check(nodes.size()==3,"unexpected trigger count");auto n=nodes.front();Check(n->name=="thorn flag delivery"&&n->action->name=="bg move to objective"&&n->action->priority>90&&n->action->priority<100,"carrier/survival priorities");for(auto node:nodes)delete node;}
 p.bg=&bg;bg.carrier=99;ThornObjectiveTravel travel{&p};Check(travel.IsActive(),"assignment travel lost");
 p.combat=true;p.victim=&p;p.distance=30;Check(!travel.IsActive(),"nearby objective combat suppressed");
 p.distance=46;Check(travel.IsActive(),"chase escaped assignment leash");
 p.distance=30;p.victimDistance=20;Check(travel.IsActive(),"distant target chase outranks objective");
 p.distance=10;Check(!travel.IsActive(),"arrived defender cannot fight");
 p.distance=100;p.casting=true;Check(!travel.IsActive(),"assignment interrupted cast");p.casting=false;
 for(unsigned type:{0u,1u,2u,3u,4u,5u}){bg.type=type;Check(!travel.IsActive(),"TG chase policy leaked to other BG");}
 bg.type=6;p.combat=false;p.distance=100;p.victimDistance=20;Player enemy;enemy.distance=30;p.enemy=&enemy;
 ThornCarrierIntercept intercept{&p};Check(intercept.IsActive(),"nearby carrier not intercepted");
 enemy.distance=46;Check(!intercept.IsActive(),"interceptor left assigned objective");enemy.distance=30;
 p.victimDistance=36;Check(!intercept.IsActive(),"intercept exceeded local combat radius");p.victimDistance=20;
 bg.carrier=7;Check(!intercept.IsActive(),"own carrier chased enemy");bg.carrier=99;
 p.enemy=nullptr;Check(!intercept.IsActive(),"missing carrier");p.enemy=&enemy;
 for(unsigned type:{0u,1u,2u,3u,4u,5u}){bg.type=type;Check(!intercept.IsActive(),"intercept leaked to other BG");}
 std::cout<<"Native TG-only carrier priority and eligibility passed\n";
}
