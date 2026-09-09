// Native BG position handler and noncombat strategy reset block.
#include <vector>
#include <map>
#include <set>
#include <string>
#include <cstdarg>
#include <cstdint>
#include <stdexcept>
#include <iostream>
using uint32=unsigned;using uint8=uint8_t;using ObjectGuid=uint64_t;
using BattleGroundTypeId=unsigned;
#define MANGOSBOT_ZERO
constexpr unsigned BATTLEGROUND_AV=1,BATTLEGROUND_WS=2,BATTLEGROUND_AB=3,BATTLEGROUND_TG=6,ALLIANCE=469,HORDE=67,MSG_BATTLEGROUND_PLAYER_POSITIONS=1;
struct WorldPacket:std::vector<double>{WorldPacket(unsigned=0){}size_t wpos(){return size();}template<class T>WorldPacket& operator<<(T v){push_back(double(v));return *this;}template<class T>void put(size_t i,T v){at(i)=v;}};
struct BattleGround;
struct Player {
    BattleGround* bg=nullptr;unsigned team=ALLIANCE;bool world=true;ObjectGuid guid=123;int group=1;
    BattleGround* GetBattleGround()const{return bg;}bool InBattleGround(){return bg!=nullptr;}
    unsigned GetBattleGroundTypeId();unsigned GetTeam()const{return team;}int GetGroup(){return group;}
    bool IsInWorld()const{return world;}ObjectGuid GetObjectGuid()const{return guid;}
    float GetPositionX()const{return 2174.5f;}float GetPositionY()const{return 1569.5f;}
};
struct Map {std::map<ObjectGuid,Player*> players;Player* GetPlayer(ObjectGuid g){auto it=players.find(g);return it==players.end()?nullptr:it->second;}}sObjectMgr;
struct BattleGround {
    unsigned type=BATTLEGROUND_TG;Map map;ObjectGuid carrier=123;
    struct Member{unsigned PlayerTeam;};std::map<ObjectGuid,Member> players;
    unsigned GetTypeID(){return type;}Map* GetBgMap(){return &map;}ObjectGuid GetFlagCarrierGuid(){return carrier;}
    int GetBgRaid(unsigned){return 1;}auto const& GetPlayers(){return players;}
};
struct BattleGroundWS:BattleGround {ObjectGuid GetHordeFlagPickerGuid(){return 123;}ObjectGuid GetAllianceFlagPickerGuid(){return 456;}};
unsigned Player::GetBattleGroundTypeId(){return bg->type;}
struct WorldSession {
    Player* _player;WorldPacket sent;unsigned sends=0;
    void SendPacket(WorldPacket* p){sent=*p;++sends;}
    void HandleBattleGroundPlayerPositionsOpcode(WorldPacket&);
};
#include "BattlegroundPositionsNative.inc"
struct Engine {
    std::set<std::string> strategies;
    void addStrategy(char const* s){strategies.insert(s);}void removeStrategy(char const* s){strategies.erase(s);}
    void addStrategies(char const* first,...){va_list args;va_start(args,first);for(auto s=first;s;s=va_arg(args,char const*))addStrategy(s);va_end(args);}
};
void ResetBgStrategies(Player* player,Engine* nonCombatEngine){
#include "BattlegroundNonCombatNative.inc"
void Check(bool ok,char const* why){if(!ok)throw std::runtime_error(why);}
int main(){
    BattleGround bg;Player viewer,carrier;viewer.bg=&bg;carrier.bg=&bg;bg.map.players[123]=&carrier;
    WorldSession session{&viewer};WorldPacket request;
    for(unsigned team:{ALLIANCE,HORDE}){viewer.team=team;session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sent.size()==5&&session.sent[1]==1&&session.sent[2]==123,"both teams need neutral carrier");}
    carrier.world=false;session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sent.size()==2&&session.sent[1]==0,"offline carrier leaked");
    carrier.world=true;BattleGround other;carrier.bg=&other;session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sent[1]==0,"foreign BG carrier leaked");
    carrier.bg=&bg;bg.carrier=999;session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sent[1]==0,"missing carrier leaked");
    bg.type=BATTLEGROUND_AB;session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sent[1]==0,"AB acquired a flag");
    BattleGroundWS ws;ws.type=BATTLEGROUND_WS;viewer.bg=&ws;sObjectMgr.players[123]=&carrier;sObjectMgr.players[456]=&carrier;
    session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sent[1]==1,"WS flag changed");
    viewer.bg=nullptr;unsigned sends=session.sends;session.HandleBattleGroundPlayerPositionsOpcode(request);Check(session.sends==sends,"outside BG sends packet");
    for(unsigned type:{BATTLEGROUND_TG,BATTLEGROUND_AB,BATTLEGROUND_WS,BATTLEGROUND_AV}){
        bg.type=type;viewer.bg=&bg;Engine engine;
        for(unsigned reset=0;reset<3;++reset){engine.strategies={"travel","rpg","follow"};ResetBgStrategies(&viewer,&engine);
            Check(engine.strategies.count("pvp")&&engine.strategies.count("battleground"),"reset lost PvP/objectives");
            Check(!engine.strategies.count("travel")&&!engine.strategies.count("rpg"),"world strategy leaked");}
    }
    viewer.bg=nullptr;Engine world;ResetBgStrategies(&viewer,&world);Check(world.strategies.empty(),"world PvP behavior changed");
    std::cout<<"Native carrier membership/team handling and repeated BG PvP strategy resets passed\n";
}
