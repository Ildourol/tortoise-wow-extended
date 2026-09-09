#include "ThornGorgeRules.h"
#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>
using uint32=unsigned;
enum Team { TEAM_NONE=0, HORDE=67, ALLIANCE=469 };
enum { QUEST_STATUS_NONE, QUEST_STATUS_INCOMPLETE, QUEST_STATUS_COMPLETE };
Team NativeTeam(ThornGorge::Team t) { return t==ThornGorge::Alliance ? ALLIANCE : t==ThornGorge::Horde ? HORDE : TEAM_NONE; }
struct BattleGroundTG;
struct Player {
    bool world=true,gm=false,dead=false;
    BattleGroundTG* bg=nullptr;
    ThornGorge::Team side=ThornGorge::Alliance;
    Team faction=ALLIANCE;
    unsigned status=QUEST_STATUS_INCOMPLETE,credits=0,lastQuest=0;
    bool IsInWorld()const{return world;} bool IsGameMaster()const{return gm;}
    BattleGroundTG* GetBattleGround()const{return bg;}
    Team GetTeam()const{return faction;}
    unsigned GetQuestStatus(unsigned)const{return status;}
    void AreaExploredOrEventHappens(unsigned q){++credits;lastQuest=q;status=QUEST_STATUS_COMPLETE;}
};
struct Map { std::map<unsigned,Player*> players; Player* GetPlayer(unsigned id){return players[id];} };
using WorldPacket=std::vector<std::pair<unsigned,unsigned>>;
constexpr unsigned ScoreStates[2]={3601,3602},BaseStates[2]={3621,3622};
struct BattleGroundTG {
    std::map<unsigned,unsigned> m_Players;
    Map map;
    ThornGorge::Rules m_rules;
    WorldPacket updates;
    Map* GetBgMap(){return &map;}
    ThornGorge::Team Side(Player* p){return p->side;}
    void Trace(char const*,Player* =nullptr,unsigned=0,char const* ="-",bool=false){}
    void RewardVictoryQuests(Team);
    void FillInitialWorldStates(WorldPacket&,uint32&);
    void SendNodeStates(unsigned);
    void FillInitialWorldState(WorldPacket& p,unsigned& n,unsigned id,unsigned value){p.emplace_back(id,value);++n;}
    void UpdateWorldState(unsigned id,unsigned value){updates.emplace_back(id,value);}
};
#include "ThornGorgeRewardsNative.inc"
#include "ThornGorgePresentationNative.inc"
void check(bool b){if(!b)throw std::runtime_error("presentation/reward regression");}
int main(){
    BattleGroundTG bg;
    Player players[7];
    for(unsigned i=0;i<7;++i){players[i].bg=&bg;bg.map.players[i]=&players[i];bg.m_Players[i]=0;}
    players[1].side=ThornGorge::Horde; players[1].faction=HORDE;
    players[2].gm=true; players[3].world=false; players[4].bg=nullptr;
    players[5].status=QUEST_STATUS_NONE; players[6].dead=true;
    bg.m_Players[7]=0; // missing/offline member
    bg.RewardVictoryQuests(TEAM_NONE); check(players[0].credits==0);
    bg.RewardVictoryQuests(ALLIANCE);
    check(players[0].credits==1 && players[0].lastQuest==42098 && players[6].credits==1);
    for(unsigned i=1;i<6;++i)check(players[i].credits==0);
    bg.RewardVictoryQuests(ALLIANCE); check(players[0].credits==1);
    bg.RewardVictoryQuests(HORDE); check(players[1].credits==1 && players[1].lastQuest==42099);
    for(unsigned owner=0;owner<3;++owner){
        bg.m_rules.owner.fill(ThornGorge::Team(owner));
        WorldPacket initial; unsigned count=0;bg.FillInitialWorldStates(initial,count);check(count==initial.size());
        std::map<unsigned,unsigned> values(initial.begin(),initial.end());
        for(unsigned node=0;node<4;++node){
            bg.updates.clear();bg.SendNodeStates(node);check(bg.updates.size()==3);
            unsigned active=0;
            for(auto kv:bg.updates){check(values.at(kv.first)==kv.second);active+=kv.second;}
            check(active==1 && values.at(ThornGorge::NodeIconState(node,ThornGorge::Team(owner)))==1);
        }
    }
    std::cout<<"Native quest filtering, repeat protection and initial/update map icons passed\n";
}
