#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
using uint32=uint32_t;using int32=int32_t;
namespace TurtleDiagnostics {std::atomic<bool> enabled{false};}
struct BoundedBotThrottle {bool Allow(uint32,uint32,uint32){return false;}};
uint32 getMSTime(){return 0;}
#define LOG_INFO(...) ((void)0)
struct GossipMenu {
    std::vector<uint32> actions;uint32 id=1;
    bool Empty(){return actions.empty();} uint32 MenuItemCount(){return actions.size();}
    uint32 MenuItemAction(uint32 i){return actions.at(i);} uint32 GetMenuId(){return id;}
};
struct Talk {GossipMenu menu;GossipMenu& GetGossipMenu(){return menu;}};
struct WorldPacket {
    std::vector<uint32> fields;
    WorldPacket& operator<<(uint32 v){fields.push_back(v);return *this;}
    WorldPacket& operator<<(std::string const&){return *this;}
};
struct Session {
    Talk talk;std::vector<uint32> offered;int selects=0;uint32 selectedAction=0;uint32 selectedIndex=999;
    void HandleGossipHelloOpcode(WorldPacket&){talk.menu.actions=offered;}
    void HandleGossipSelectOptionOpcode(WorldPacket& p){
        if(p.fields.size()!=2) throw std::runtime_error("Expected 1.12 GUID/index packet");
        selectedIndex=p.fields.at(1);selectedAction=talk.menu.MenuItemAction(selectedIndex);
        ++selects;talk.menu.actions.clear();
    }
};
struct Creature {uint32 GetObjectGuid(){return 42;}};
struct Guid {uint32 GetRawValue(){return 1;}};
struct Player {Session session;Talk* PlayerTalkClass=&session.talk;void SetFacingToObject(Creature*){}Session* GetSession(){return &session;}Guid GetObjectGuid(){return {};}};
struct Scripts {int direct=0;bool OnGossipSelect(Player*,Creature*,uint32,uint32,const char*){++direct;return true;}} sScriptMgr;
struct DungeonEventExecutor {static bool SelectGossip(Player*,Creature*,int32);};
#include "NativeDungeonGossip.inc"
void check(bool v){if(!v)throw std::runtime_error("dungeon gossip failed");}
int main(){
    Player bot;Creature npc;
    bot.session.offered={1001};
    check(DungeonEventExecutor::SelectGossip(&bot,&npc,1001));
    check(bot.session.selects==1 && bot.session.selectedIndex==0 && bot.session.selectedAction==1001 && sScriptMgr.direct==0);
    bot.session.offered={1002};
    check(!DungeonEventExecutor::SelectGossip(&bot,&npc,1001));check(bot.session.selects==1 && sScriptMgr.direct==0);
    bot.session.offered={1000,1001};
    check(DungeonEventExecutor::SelectGossip(&bot,&npc,1));check(bot.session.selectedIndex==1 && bot.session.selectedAction==1001);
    check(!DungeonEventExecutor::SelectGossip(&bot,&npc,-1));check(!DungeonEventExecutor::SelectGossip(nullptr,&npc,0));
    // Existing script-only empty-menu compatibility remains explicitly scoped.
    bot.session.offered.clear();check(DungeonEventExecutor::SelectGossip(&bot,&npc,0));check(sScriptMgr.direct==1);
}
