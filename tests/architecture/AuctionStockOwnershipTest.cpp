// Runs native publication, property/enchantment mutation, state and queue methods.
// Persistence, lookup and allocation are doubles; no live database is required.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "MarketPolicy.h"
using uint32 = uint32_t; using int32 = int32_t; using uint64 = uint64_t;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
constexpr uint32 HIGHGUID_PLAYER = 0, HOUR = 3600;
constexpr uint32 ITEM_FIELD_RANDOM_PROPERTIES_ID = 0, ITEM_FIELD_ENCHANTMENT = 1;
constexpr uint32 MAX_ENCHANTMENT_OFFSET = 3, ENCHANTMENT_ID_OFFSET = 0;
constexpr uint32 ENCHANTMENT_DURATION_OFFSET = 1, ENCHANTMENT_CHARGES_OFFSET = 2;
enum EnchantmentSlot { PROP_ENCHANTMENT_SLOT_0 = 0 };
enum ItemUpdateState { ITEM_NEW, ITEM_CHANGED, ITEM_UNCHANGED, ITEM_REMOVED };
struct ObjectGuid {
    uint32 low = 0;
    ObjectGuid() = default; ObjectGuid(uint32, uint32 id) : low(id) {}
    bool IsEmpty() const { return !low; }
    bool operator!=(ObjectGuid other) const { return low != other.low; }
    std::string GetString() const { return std::to_string(low); }
};
class Item;
struct Player {
    std::vector<Item*> m_itemUpdateQueue;
    bool m_itemUpdateQueueBlocked = false;
    ObjectGuid GetObjectGuid() const { return {0, 10}; }
    std::string GetGuidStr() const { return "10"; }
} player;
bool online = true, hardcore = false, createFails = false;
int32 generatedProperty = 153;
uint32 allocationCount = 0, nextGuid = 0;
struct Row { uint32 owner, entry, count; int32 property; };
std::map<uint32, Row> rows;
struct Log { template<class... T> void outError(char const*, T...) {} } sLog;
struct ItemRandomPropertiesEntry { uint32 ID = 153; uint32 enchant_id[3] = {11, 12, 13}; } propertyEntry;
struct PropertyStore {
    ItemRandomPropertiesEntry const* LookupEntry(uint32 id) { return id == 153 ? &propertyEntry : nullptr; }
} sItemRandomPropertiesStore;
class Item {
public:
    uint32 guid = ++nextGuid, entry, count, fields[10]{};
    ObjectGuid owner;
    ItemUpdateState uState = ITEM_NEW; int uQueuePos = -1;
    Item(uint32 e, uint32 c) : entry(e), count(c) { ++allocationCount; }
    ~Item() { --allocationCount; }
    static Item* CreateItem(uint32 e, uint32 c) { return createFails ? nullptr : new Item(e,c); }
    static int32 GenerateItemRandomPropertyId(uint32) { return generatedProperty; }
    void SetOwnerGuid(ObjectGuid g) { owner = g; }
    ObjectGuid GetOwnerGuid() const { return owner; }
    Player* GetOwner() { return online && owner.low == 10 ? &player : nullptr; }
    std::string GetGuidStr() const { return std::to_string(guid); }
    bool IsInUpdateQueue() const { return uQueuePos != -1; }
    int32 GetInt32Value(uint32 i) const { return fields[i]; }
    void SetInt32Value(uint32 i, int32 v) { fields[i] = v; }
    void SetUInt32Value(uint32 i, uint32 v) { fields[i] = v; }
    uint32 GetEnchantmentId(EnchantmentSlot s) const { return fields[1+3*s]; }
    uint32 GetEnchantmentDuration(EnchantmentSlot s) const { return fields[2+3*s]; }
    uint32 GetEnchantmentCharges(EnchantmentSlot s) const { return fields[3+3*s]; }
    void SetEnchantment(EnchantmentSlot, uint32, uint32, uint32);
    void SetItemRandomProperties(int32);
    void SetState(ItemUpdateState, Player* = nullptr);
    void AddToUpdateQueueOf(Player* = nullptr);
    void RemoveFromUpdateQueueOf(Player* = nullptr);
    void ClearUpdateMask(bool) {} // Object update mask is independent of item save queue.
    uint32 GetGUIDLow() const { return guid; }
    int32 GetItemRandomPropertyId() const { return fields[0]; }
    void SaveToDB() {
        rows[guid] = {owner.low,entry,count,GetItemRandomPropertyId()};
        SetState(ITEM_UNCHANGED); // Native SaveToDB's final state transition.
    }
};
#include "NativeAuctionItemQueue.inc"
#include "NativeAuctionEnchantment.inc"
struct AuctionHouseEntry {} houseEntry;
struct AuctionEntry {
    uint32 Id, itemGuidLow, itemTemplate, itemCount, owner, ownerAccount, buyout, startbid;
    int32 itemRandomPropertyId; time_t depositTime, expireTime;
    AuctionHouseEntry const* auctionHouseEntry;
    void SaveToDB() {}
};
struct HouseStore { AuctionHouseEntry* LookupEntry(uint32) { return &houseEntry; } } sAuctionHouseStore;
struct HouseType {
    std::vector<std::unique_ptr<AuctionEntry>> auctions;
    void AddAuction(AuctionEntry* a) { auctions.emplace_back(a); }
} house;
HouseType* House(uint32) { return &house; }
struct AuctionManager {
    std::map<uint32,std::unique_ptr<Item>> items;
    void AddAItem(Item* item) { items.emplace(item->guid,std::unique_ptr<Item>(item)); }
} sAuctionMgr;
struct ObjectManager {
    uint32 next = 0;
    uint32 GetPlayerAccountIdByGUID(uint32) { return 20; }
    uint32 GenerateAuctionID() { return ++next; }
} sObjectMgr;
struct Database { void BeginTransaction() {} void CommitTransaction() {} } CharacterDatabase;
uint32 urand(uint32 a, uint32) { return a; }
bool IsPlayerHardcore(uint32) { return hardcore; }
namespace ahbot {
struct AhBot {
    struct Owner { uint32 guid, account; };
    std::vector<Owner> owners{{10,20}};
    struct Settings { uint32 bidMin=75,bidMax=90,timeMin=2,timeMax=24; } settings;
    uint32 currentHouse = 0; static uint32 auctionIds[3];
    bool IsBotOwner(uint32 g,uint32 a) { return g==10 && a==20; }
    bool Publish(uint32,uint32,uint32);
};
uint32 AhBot::auctionIds[3] = {1,6,7};
}
using ahbot::AhBot;
#include "NativeAuctionPublish.inc"
int main() {
    // Establish the original failure using the real queue/state implementation.
    auto old = std::make_unique<Item>(15426,1);
    old->SetOwnerGuid({0,10}); old->SetItemRandomProperties(153);
    CHECK(player.m_itemUpdateQueue.size()==1);
    CHECK(player.m_itemUpdateQueue[0]==old.get());
    old->SaveToDB();
    CHECK(!old->IsInUpdateQueue()); // Position lost while raw pointer remains queued.
    CHECK(player.m_itemUpdateQueue[0]==old.get());
    player.m_itemUpdateQueue.clear(); rows.clear(); old.reset();

    AhBot bot;
    for (bool connected : {false,true}) {
        online = connected;
        for (int32 property : {0,153,999}) {
            generatedProperty = property;
            for (unsigned repetition=0; repetition<100; ++repetition) {
                CHECK(bot.Publish(15426,2,100));
                auto const& a = *house.auctions.back();
                auto& item = *sAuctionMgr.items.at(a.itemGuidLow);
                CHECK(player.m_itemUpdateQueue.empty()); CHECK(!item.IsInUpdateQueue());
                CHECK(item.owner.low==10); CHECK(item.uState==ITEM_UNCHANGED);
                CHECK(rows.at(item.guid).owner==10); CHECK(rows.at(item.guid).count==2);
                CHECK(item.GetItemRandomPropertyId()==(property==153 ? 153 : 0));
                CHECK(a.itemRandomPropertyId==item.GetItemRandomPropertyId());
                if (property==153) CHECK(item.GetEnchantmentId(PROP_ENCHANTMENT_SLOT_0)==11);
                // Owner save/logout has no auction pointer to process; repeated save preserves stock.
                item.SaveToDB(); CHECK(player.m_itemUpdateQueue.empty());
            }
        }
    }
    auto count=allocationCount; auto listed=house.auctions.size();
    createFails=true; CHECK(!bot.Publish(15426,1,100)); createFails=false;
    hardcore=true; CHECK(!bot.Publish(15426,1,100)); hardcore=false;
    bot.owners.clear(); CHECK(!bot.Publish(15426,1,100));
    CHECK(allocationCount==count); CHECK(house.auctions.size()==listed);
    // Ordinary inventory items must still queue when enchanted for an online owner.
    auto inventory=std::make_unique<Item>(15426,1); online=true;
    inventory->SetOwnerGuid({0,10}); inventory->SetItemRandomProperties(153);
    CHECK(player.m_itemUpdateQueue.size()==1); CHECK(player.m_itemUpdateQueue[0]==inventory.get());
    inventory->RemoveFromUpdateQueueOf(&player); player.m_itemUpdateQueue.clear(); inventory.reset();
    house.auctions.clear(); sAuctionMgr.items.clear(); CHECK(allocationCount==0);
    std::cout << "Native auction publication/item queue ownership passed\n";
}
