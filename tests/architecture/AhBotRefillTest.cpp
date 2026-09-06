// Exercise production refill selection with deterministic item/auction services.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
using uint32 = uint32_t;
using int32 = int32_t;
uint32 urand(uint32 a, uint32) { return a; }
struct Log { template<class... T> void outDetail(T...) {} template<class... T> void outString(T...) {} } sLog;
struct ItemPrototype { uint32 ItemId; std::string Name1 = "test"; };
struct ObjectMgr {
    std::map<uint32, ItemPrototype> items;
    ItemPrototype const* GetItemPrototype(uint32 id) { auto i=items.find(id); return i==items.end()?nullptr:&i->second; }
} sObjectMgr;
struct Config { uint32 maxSellInterval = 28800; } sAhBotConfig;
struct Category {
    int32 perItem = 2;
    std::string GetDisplayName() { return "test"; }
    int32 GetMaxAllowedItemAuctionCount(ItemPrototype const*) { return perItem; }
};
struct ItemBag {
    std::vector<uint32> items;
    std::vector<uint32>& Get(Category*) { return items; }
    int32 GetCount(Category*) { return int32(items.size()); }
    int32 GetCount(Category*, uint32 id) { return int32(std::count(items.begin(), items.end(),id)); }
    void Add(ItemPrototype const* p) { items.push_back(p->ItemId); }
};
struct AhBot {
    struct ItemOverride { uint32 addChance = 0; };
    ItemBag availableItems;
    std::map<std::string,int32> categoryMaxAuctionCount;
    std::atomic<uint32> rebuildPassesRemaining{0};
    uint32 auctionIds[3]{1,6,7};
    int timerCalls = 0, listingCalls = 0;
    std::set<uint32> failItems;
    bool GetItemOverride(uint32, ItemOverride&) { return false; }
    uint32 GetSellTime(uint32, uint32, Category*&) { ++timerCalls; return uint32(time(nullptr))+7200; }
    int AddAuction(int, Category*, ItemPrototype const* p) { ++listingCalls; return failItems.count(p->ItemId)?0:1; }
    int AddAuctions(int, Category*, ItemBag*);
};
#include "NativeAhBotRefill.inc"
#define CHECK(x) do { if(!(x)) { std::cerr << "line " << __LINE__ << ": " #x << '\n'; std::exit(1); } } while(0)
int main() {
    for(uint32 id=1;id<=4;++id) sObjectMgr.items[id] = {id};
    Category cat;
    { // Normal sale decisions must continue honoring future timers.
        AhBot bot; ItemBag stock;
        bot.availableItems.items={1,2}; bot.categoryMaxAuctionCount["test"]=4;
        CHECK(bot.AddAuctions(0,&cat,&stock)==0);
        CHECK(bot.timerCalls>0 && bot.listingCalls==0 && stock.items.empty());
    }
    { // Rebuild reaches capacity even when earliest candidates are capped.
        AhBot bot; ItemBag stock;
        stock.items={1,1}; bot.availableItems.items={1,2,3,4};
        bot.categoryMaxAuctionCount["test"]=7; bot.rebuildPassesRemaining=3;
        CHECK(bot.AddAuctions(0,&cat,&stock)==5);
        CHECK(stock.items.size()==7 && bot.timerCalls==0);
        CHECK(stock.GetCount(&cat,1)==2 && stock.GetCount(&cat,2)==2);
        CHECK(bot.AddAuctions(0,&cat,&stock)==0);
    }
    { // Bad templates and failed listings never consume capacity.
        AhBot bot; ItemBag stock;
        bot.availableItems.items={99,1,2}; bot.failItems={1};
        bot.categoryMaxAuctionCount["test"]=4; bot.rebuildPassesRemaining=3;
        CHECK(bot.AddAuctions(0,&cat,&stock)==2);
        CHECK(stock.items==std::vector<uint32>({2,2}));
        CHECK(bot.listingCalls==3 && bot.timerCalls==0);
    }
    { // Empty pool and disabled category end without attempting a listing.
        AhBot bot; ItemBag stock; bot.categoryMaxAuctionCount["test"]=0;
        CHECK(bot.AddAuctions(0,&cat,&stock)==0);
        bot.categoryMaxAuctionCount["test"]=10; bot.rebuildPassesRemaining=3;
        CHECK(bot.AddAuctions(0,&cat,&stock)==0 && bot.listingCalls==0);
    }
    std::cout << "Native AHBot refill pacing/caps/failure tests passed\n";
}
