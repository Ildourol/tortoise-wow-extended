// Native command/scheduler/worker bodies; no live auction or database writes.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using uint32 = uint32_t;
#define MAX_AUCTIONS 3
time_t testTime = 1000;
time_t fakeTime(time_t*) { return testTime; }
#define time fakeTime
struct Log { template<class... T> void outString(T...) {} template<class... T> void outError(T...) {} } sLog;
struct World { bool stop = false; bool IsShutdowning() { return stop; } bool IsStopped() { return stop; } } sWorld;
struct Config { bool enabled = true; uint32 updateInterval = 900; } sAhBotConfig;
struct ChatHandler {
    std::vector<std::string> messages;
    void SendSysMessage(char const* s) { messages.emplace_back(s); }
    template<class... T> void PSendSysMessage(char const* s,T...) { messages.emplace_back(s); }
};
struct Category {} cat;
struct CategoryList { static std::vector<Category*> instance; };
std::vector<Category*> CategoryList::instance{&cat};
struct InAuctionItemsBag { InAuctionItemsBag(uint32) {} void Init(bool) {} };
bool launchFails = false;
int launches = 0;
bool activateAhbotThread() { ++launches; if (launchFails) throw std::runtime_error("launch"); return true; }
struct AhBot {
    std::atomic<bool> updating{false};
    std::atomic<uint32> pendingRebuild{0}, rebuildPassesRemaining{0};
    time_t nextAICheckTime = 0;
    uint32 nextHouseIndex = 0, auctionIds[3]{1,6,7};
    std::set<uint32> allBidders{1};
    std::map<uint32,std::vector<uint32>> bidders;
    int rebuildCalls = 0, answerCalls = 0, sold = 0, queuedRuns = 0;
    bool includedPlayerBids = false, failPass = false;
    bool StartUpdate(); bool QueueRebuild(bool); bool ProcessPendingRebuild();
    void Update(); void ForceUpdate();
    bool RebuildCommand(ChatHandler*,std::string,std::string,std::string);
    uint32 Rebuild(bool all, uint32& protectedBids) {
        ++rebuildCalls; includedPlayerBids=all; protectedBids=all?0:1;
        nextHouseIndex=0; rebuildPassesRemaining=3; nextAICheckTime=time(nullptr)+65; return 1400;
    }
    void RunQueuedWork() { ++queuedRuns; }
    void CleanupPropositions() {} void LoadRandomBots() {} void CheckCategoryMultipliers() {} void CleanupHistory() {}
    int Answer(uint32,Category*,InAuctionItemsBag*) { ++answerCalls; return 0; }
    int AddAuctions(uint32,Category*,InAuctionItemsBag*) {
        if(failPass) throw std::runtime_error("pass");
        sold += rebuildPassesRemaining?6000:1; return rebuildPassesRemaining?6000:1;
    }
};
#include "NativeAhBotScheduler.inc"
#include "NativeAhBotWorker.inc"
bool AhBot::RebuildCommand(ChatHandler* handler,std::string action,std::string option,std::string extra) {
#include "NativeAhBotRebuildCommand.inc"
    return false;
}
#define CHECK(x) do { if(!(x)) { std::cerr << "line " << __LINE__ << ": " #x << '\n'; std::exit(1); } } while(0)
int main() {
    ChatHandler chat;
    { // Reproduce live timing: command during active worker, not while idle.
        AhBot bot;
        CHECK(bot.StartUpdate() && bot.updating); // worker reserved, not started yet
        CHECK(bot.RebuildCommand(&chat,"rebuild","",""));
        CHECK(bot.pendingRebuild==1 && bot.rebuildCalls==0);
        bot.Update(); CHECK(bot.rebuildCalls==0 && bot.updating);
        CHECK(!bot.StartUpdate());
        bot.ForceUpdate(); CHECK(!bot.updating && bot.answerCalls==1);
        bot.Update(); CHECK(bot.rebuildCalls==1 && !bot.includedPlayerBids);
        CHECK(bot.pendingRebuild==0 && bot.rebuildPassesRemaining==3);
        CHECK(bot.RebuildCommand(&chat,"rebuild","","")); // duplicate is harmless
        CHECK(bot.pendingRebuild==0);
        bot.Update(); CHECK(!bot.updating); // cannot skip native expiry grace
        testTime += 65;
        for (int house=0;house<3;++house) {
            bot.Update(); CHECK(bot.updating);
            bot.ForceUpdate(); CHECK(!bot.updating); ++testTime;
        }
        CHECK(bot.rebuildCalls==1 && bot.sold==18001);
        CHECK(bot.answerCalls==1 && bot.rebuildPassesRemaining==0);
        bot.Update(); bot.ForceUpdate(); CHECK(bot.answerCalls==2); // normal buying resumes
    }
    { // Explicit all is retained, never silently downgraded by a duplicate.
        AhBot bot; CHECK(bot.QueueRebuild(false)); CHECK(bot.QueueRebuild(true)); CHECK(bot.QueueRebuild(false));
        bot.Update(); CHECK(bot.rebuildCalls==1 && bot.includedPlayerBids);
        CHECK(!bot.RebuildCommand(&chat,"rebuild","typo",""));
        CHECK(!bot.RebuildCommand(&chat,"rebuild","all","extra"));
    }
    { // Failed launch releases reservation; request still executes next tick.
        AhBot bot; launchFails=true; CHECK(!bot.StartUpdate() && !bot.updating); launchFails=false;
        CHECK(bot.QueueRebuild(false)); bot.Update(); CHECK(bot.rebuildCalls==1);
    }
    { // A failed refill keeps its pass due and releases worker ownership.
        AhBot bot; bot.rebuildPassesRemaining=3; bot.failPass=true;
        CHECK(bot.StartUpdate()); bot.ForceUpdate(); CHECK(!bot.updating && bot.rebuildPassesRemaining==3);
        bot.failPass=false; CHECK(bot.StartUpdate()); bot.ForceUpdate(); CHECK(bot.rebuildPassesRemaining==2);
    }
    { // Disabled/shutdown work must not expire auctions.
        AhBot bot; CHECK(bot.QueueRebuild(false)); sWorld.stop=true;
        bot.Update(); CHECK(bot.pendingRebuild==1 && bot.rebuildCalls==0); sWorld.stop=false;
        sAhBotConfig.enabled=false; bot.Update(); CHECK(!bot.pendingRebuild && !bot.updating && bot.rebuildCalls==0);
        CHECK(!bot.RebuildCommand(&chat,"rebuild","","")); sAhBotConfig.enabled=true;
    }
    std::cout << "Native AHBot busy rebuild, expiry grace, three-house refill and failure lifecycle passed\n";
}
