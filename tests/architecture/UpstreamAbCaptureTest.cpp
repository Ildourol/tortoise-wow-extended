// Production fragments with deterministic services, not a live AB playtest.
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <string>
#include <vector>
using uint32 = std::uint32_t;
static void require(bool ok) { if (!ok) std::abort(); }
#include "UpstreamAbEntries.inc"

struct Timestamp {
    time_t value = 0;
    time_t Get() const { return value; }
    void Set(time_t t) { value = t; }
};
struct Context {
    Timestamp timestamp;
    template<class T> Timestamp* GetValue(char const* name, char const* qualifier) {
        require(std::string(name) == "last spell cast time");
        require(std::string(qualifier) == "capture banner");
        return &timestamp;
    }
};
struct BGTactics {
    Context* context;
    bool CanAttemptAbCapture();
};
static time_t now = 100;
static time_t FakeTime(void*) { return now; }
#define time FakeTime
#include "UpstreamAbThrottle.inc"
#undef time

enum { BATTLEGROUND_AB = 1, BATTLEGROUND_WS = 2, GO_STATE_READY = 0 };
struct Object {
    bool spawned, used;
    int state;
    bool IsInUse() const { return used; }
    int GetGoState() const { return state; }
};
struct Facade { bool isSpawned(Object* go) const { return go->spawned; } } sServerFacade;
static bool passesGate(Object* go, int bgType) {
    auto abSay = [](char const*) {};
    for (int one = 0; one != 1; ++one) {
#include "UpstreamAbStateGate.inc"
        return true;
    }
    return false;
}
int main() {
    require(vFlagsAB == std::vector<uint32>({180058,180059,180060,180061,180087,180088,180089,180090,180091}));
    for (uint32 index = 0; index < 5; ++index)
        require(std::find(vFlagsAB.begin(), vFlagsAB.end(), index) == vFlagsAB.end());
    for (bool spawned : {false,true})
        for (bool used : {false,true})
            for (int state : {0,1}) {
                Object object{spawned,used,state};
                require(passesGate(&object,BATTLEGROUND_AB) == spawned);
                require(passesGate(&object,BATTLEGROUND_WS) == (spawned && !used && state == GO_STATE_READY));
            }
    Context first, second;
    BGTactics action{&first}, otherActionSameBot{&first}, otherBot{&second};
    require(action.CanAttemptAbCapture());
    require(!otherActionSameBot.CanAttemptAbCapture());
    require(otherBot.CanAttemptAbCapture());
    now = 103; require(!action.CanAttemptAbCapture());
    now = 104; require(otherActionSameBot.CanAttemptAbCapture());
    now = 90; require(action.CanAttemptAbCapture()); // wall-clock rollback cannot strand the bot
    require(!action.CanAttemptAbCapture());
}
