#include <cstdint>
#include <stdexcept>
using uint8=uint8_t;using uint32=uint32_t;using int32=int32_t;
constexpr unsigned QUEST_OBJECTIVES_COUNT=4,QUEST_STATUS_INCOMPLETE=1;
constexpr unsigned GOSSIP_ACTION_INFO_DEF=1000,GOSSIP_SENDER_MAIN=0,GOSSIP_ICON_TALK=0;
struct Quest { int32 ReqCreatureOrGOId[4]={60075,60076,60077,0};uint32 ReqCreatureOrGOCount[4]={1,1,1,0}; };
struct QuestStatusData {uint32 m_status=1,m_creatureOrGOcount[4]={};};
struct ObjectMgr {Quest quest;bool present=true;Quest const* GetQuestTemplate(uint32) {return present?&quest:nullptr;}} sObjectMgr;
struct Talk {void ClearMenus(){}};
struct Player {
    QuestStatusData status;bool hasQuest=true;int offers=0,credits=0;Talk talk;Talk* PlayerTalkClass=&talk;
    QuestStatusData const* GetQuestStatusData(uint32) const {return hasQuest?&status:nullptr;}
    void ADD_GOSSIP_ITEM(unsigned,const char*,unsigned,unsigned){++offers;}
    void SEND_GOSSIP_MENU(unsigned,unsigned){} void CLOSE_GOSSIP_MENU(){}
    void KilledMonsterCredit(unsigned entry){++credits;++status.m_creatureOrGOcount[entry-60075];}
};
struct GameObject {unsigned guid,entry;unsigned GetDBTableGUIDLow() const{return guid;} unsigned GetEntry() const{return entry;}unsigned GetObjectGuid()const{return guid;}};
struct GameObjectScript {
    explicit GameObjectScript(const char*){} virtual ~GameObjectScript()=default;
    virtual bool OnGossipHello(Player*,GameObject*){return false;}
    virtual bool OnGossipSelect(Player*,GameObject*,uint32,uint32){return false;}
};
#include "NativeBalorGossip.inc"
void check(bool value){if(!value)throw std::runtime_error("Balor gossip failed");}
int main(){
    go_balor_explosives script;
    GameObject objects[]={{5022619,2020178},{5022613,2020179},{5022612,2020180}};
    Player player;
    for(auto& go:objects){
        check(script.OnGossipHello(&player,&go));
        check(script.OnGossipSelect(&player,&go,0,1001));
        check(script.OnGossipSelect(&player,&go,0,1001)); // no repeat credit
    }
    check(player.offers==3 && player.credits==3);
    Player inactive;inactive.hasQuest=false;script.OnGossipHello(&inactive,&objects[0]);script.OnGossipSelect(&inactive,&objects[0],0,1001);
    check(inactive.offers==0 && inactive.credits==0);
    inactive.hasQuest=true;inactive.status.m_status=2;script.OnGossipSelect(&inactive,&objects[0],0,1001);check(inactive.credits==0);
    inactive.status.m_status=1;sObjectMgr.present=false;script.OnGossipSelect(&inactive,&objects[0],0,1001);check(inactive.credits==0);
    sObjectMgr.present=true;sObjectMgr.quest.ReqCreatureOrGOId[0]=123;
    script.OnGossipSelect(&inactive,&objects[0],0,1001);check(inactive.credits==0);
    GameObject unrelated{5022619,999};check(!script.OnGossipHello(&player,&unrelated));
    check(!script.OnGossipSelect(&player,&objects[0],1,1001));check(!script.OnGossipSelect(&player,&objects[0],0,999));
}
