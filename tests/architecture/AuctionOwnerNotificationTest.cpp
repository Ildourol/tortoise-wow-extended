// Executes the production notification and expiry/success mail functions.
// Mock transport/persistence expose side effects; this is not a crash replay.
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using uint32=uint32_t;using uint64=uint64_t;
constexpr uint32 HIGHGUID_PLAYER=0,SMSG_AUCTION_OWNER_NOTIFICATION=0x25f;
constexpr uint32 LOG_MAIL_AH=1,AUCTION_SUCCESSFUL=2,AUCTION_EXPIRED=3,MAIL_CHECK_MASK_COPIED=4;
#define DEBUG_LOG(...) ((void)0)
void check(bool value){if(!value)throw std::runtime_error("auction owner notification contract failed");}
struct ObjectGuid {
    uint64 value=0;ObjectGuid()=default;ObjectGuid(uint32,uint32 low):value(low){}
    uint32 GetCounter() const{return static_cast<uint32>(value);}
};
struct WorldPacket {
    std::vector<uint64> fields;
    WorldPacket(uint32 opcode,uint32){check(opcode==SMSG_AUCTION_OWNER_NOTIFICATION);}
    WorldPacket& operator<<(uint32 value){fields.push_back(value);return *this;}
    WorldPacket& operator<<(ObjectGuid guid){fields.push_back(guid.value);return *this;}
};
struct AuctionEntry {
    uint32 Id=17,bid=0,bidder=0,itemTemplate=99,itemGuidLow=123,owner=42,buyout=500,deposit=20;
    uint32 GetAuctionOutBid()const{return bid ? (bid/100)*5 : 1;}
    uint32 GetAuctionCut()const{return 25;}
};
struct Item {
    uint32 GetItemRandomPropertyId()const{return 77;}
    uint32 GetGUIDLow()const{return 123;}
    uint32 GetEntry()const{return 99;}
};
struct Player;
struct WorldSession {
    bool connected=false;Player* player=nullptr;std::vector<std::vector<uint64>> packets;
    void* GetSocket(){return connected?this:nullptr;}
    Player* GetPlayer(){return player;}
    uint32 GetAccountId(){return 12;}
    void SendPacket(WorldPacket* packet){packets.push_back(packet->fields);}
    void SendAuctionOwnerNotification(AuctionEntry*,bool);
};
struct Player {
    WorldSession session;bool hardcore=false;
    Player(){session.player=this;}
    WorldSession* GetSession(){return &session;}
    bool IsHardcore(){return hardcore;}
    std::string GetShortDescription(){return "owner";}
};
struct ObjectMgr {
    Player* online=nullptr;uint32 account=12;
    Player* GetPlayer(ObjectGuid){return online;}
    uint32 GetPlayerAccountIdByGUID(ObjectGuid){return account;}
}sObjectMgr;
bool IsPlayerHardcore(uint32){return false;}
struct Log {
    int messages=0;
    template<class... A>void out(A...){++messages;}
    template<class... A>void outError(A...){++messages;}
}sLog;
struct Database {template<class... A>void PExecute(A...){throw std::runtime_error("unexpected item deletion");}}CharacterDatabase;
struct MailReceiver {Player* player;ObjectGuid guid;MailReceiver(Player* p,ObjectGuid g):player(p),guid(g){}};
struct Delivery {uint32 receiver,money;Item* item;std::string subject;};
std::vector<Delivery> deliveries;
struct MailDraft {
    std::string subject;uint32 money=0;Item* item=nullptr;
    explicit MailDraft(std::string s):subject(s){}
    MailDraft(std::string s,std::string):subject(s){}
    MailDraft& SetMoney(uint32 n){money=n;return *this;}
    MailDraft& AddItem(Item* i){item=i;return *this;}
    void SendMailTo(MailReceiver receiver,AuctionEntry*,uint32){deliveries.push_back({receiver.guid.GetCounter(),money,item,subject});}
};
struct AuctionHouseMgr {
    Item item;bool itemPresent=true;int lookups=0,removed=0;
    Item* GetAItem(uint32){++lookups;return itemPresent?&item:nullptr;}
    void RemoveAItem(uint32){++removed;}
    void SendAuctionExpiredMail(AuctionEntry*);
    void SendAuctionSuccessfulMail(AuctionEntry*);
}sAuctionMgr;
#include "NativeAuctionOwnerNotification.inc"
#include "NativeAuctionOwnerMail.inc"
int main(){
    AuctionEntry auction;Player owner;sObjectMgr.online=&owner;
    // Socketless clients skip all notification construction, lookups and hooks.
    for(bool sold:{false,true}){
        owner.session.SendAuctionOwnerNotification(&auction,sold);
        check(owner.session.packets.empty() && sAuctionMgr.lookups==0 && sLog.messages==0);
    }
    for(bool connected:{false,true}){
        owner.session.connected=connected;owner.session.packets.clear();deliveries.clear();
        auction.bid=0;auction.bidder=0;
        // Unsold expiry still returns the exact item to the owner by mail.
        sAuctionMgr.SendAuctionExpiredMail(&auction);
        check(deliveries.size()==1 && deliveries[0].receiver==42 && deliveries[0].item==&sAuctionMgr.item);
        check(deliveries[0].subject=="99:0:3");
        check(owner.session.packets.size()==(connected?1:0));
        if(connected)check(owner.session.packets.back()==std::vector<uint64>({17,0,1,0,99,77}));
        // Sale still delivers bid+deposit-cut proceeds; connected-client bytes
        // retain the sold GUID convention.
        owner.session.packets.clear();deliveries.clear();auction.bid=500;auction.bidder=84;
        sAuctionMgr.SendAuctionSuccessfulMail(&auction);
        check(deliveries.size()==1 && deliveries[0].receiver==42 && deliveries[0].money==495 && !deliveries[0].item);
        check(deliveries[0].subject=="99:0:2");
        check(owner.session.packets.size()==(connected?1:0));
        if(connected)check(owner.session.packets.back()==std::vector<uint64>({17,500,25,0,99,77}));
        // Ordinary bid update retains bidder identity (no expiry or settlement).
        owner.session.packets.clear();deliveries.clear();auction.bid=100;
        owner.session.SendAuctionOwnerNotification(&auction,false);
        check(deliveries.empty() && owner.session.packets.size()==(connected?1:0));
        if(connected)check(owner.session.packets.back()==std::vector<uint64>({17,100,5,84,99,77}));
    }
    // Logged-out owners retain the native offline-mail path.
    sObjectMgr.online=nullptr;deliveries.clear();sAuctionMgr.SendAuctionExpiredMail(&auction);
    check(deliveries.size()==1 && deliveries[0].receiver==42 && deliveries[0].item==&sAuctionMgr.item);
    check(sAuctionMgr.removed==1);
    // Existing missing-item and missing-session-player handling stays intact.
    owner.session.connected=true;owner.session.player=nullptr;sAuctionMgr.itemPresent=false;
    owner.session.SendAuctionOwnerNotification(&auction,false);
    check(owner.session.packets.back().back()==0);
    deliveries.clear();sAuctionMgr.SendAuctionExpiredMail(&auction);check(deliveries.empty());
}
