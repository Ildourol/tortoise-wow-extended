#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>
using uint32=uint32_t; using uint64=uint64_t;
struct PlayerbotHolder {
    int calls=0; std::function<void()> callback;
    static void UpdateAllHolderSessions(uint32);
    void UpdateSessions(uint32) { ++calls; if(callback) callback(); }
};
#include "NativeHolderRegistry.inc"
#include "NativeHolderDispatch.inc"
void check(bool value) { if(!value) throw std::runtime_error("holder dispatch failed"); }
int main() {
    PlayerbotHolder a,b;
    HolderRegistry()[&a]=++HolderGeneration(); HolderRegistry()[&b]=++HolderGeneration();
    auto* first=HolderRegistry().begin()->first;
    auto* later=HolderRegistry().rbegin()->first;
    first->callback=[&] {
        std::lock_guard<std::mutex> lock(HolderRegistryLock()); // dispatch cannot hold it
        HolderRegistry().erase(later);
        HolderRegistry()[later]=++HolderGeneration(); // same address, new lifetime
    };
    PlayerbotHolder::UpdateAllHolderSessions(10);
    check(first->calls==1 && later->calls==0);
    first->callback={}; PlayerbotHolder::UpdateAllHolderSessions(10);
    check(first->calls==2 && later->calls==1);
    first->callback=[&] { HolderRegistry().erase(later); };
    PlayerbotHolder::UpdateAllHolderSessions(10);
    check(first->calls==3 && later->calls==1);
}
