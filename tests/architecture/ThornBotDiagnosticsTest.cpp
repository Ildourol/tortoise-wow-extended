#include "ThornGorgeDiagnostics.h"
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <iostream>
using uint32=unsigned;using int32=int;
constexpr uint32 STATUS_IN_PROGRESS=3,BATTLEGROUND_TG=6,LOG_BG=1;
enum ActivityType {ALL_ACTIVITY,DETAILED_MOVE_ACTIVITY,REACT_ACTIVITY};
struct MapEntry { unsigned type; bool IsBattleGround()const{return type==3;} };
struct Store {std::map<unsigned,MapEntry> rows;MapEntry const* LookupEntry(unsigned id){auto i=rows.find(id);return i==rows.end()?nullptr:&i->second;}}sMapStore;
struct WorldPosition {unsigned map;unsigned getMapId()const{return map;}bool isBg()const;};
#include "BotMapClassification.inc"
struct Player;
struct BattleGround {unsigned type=6;unsigned GetTypeId(){return type;}};
struct BattleGroundTG:BattleGround {
    ThornGorge::DiagnosticBudget m_diagnostics;
    std::map<unsigned,unsigned> m_Players,m_botDiagnosticTicks;
    unsigned m_elapsed=0,status=3;
    unsigned GetStatus(){return status;}void* GetBgMap(){return this;}
    bool AdmitBotDiagnostic(Player*);
};
struct Spline {
    bool initialized=false,done=false;
    bool Initialized(){return initialized;}
    bool Finalized(){if(!initialized)throw std::runtime_error("fresh spline read");return done;}
    unsigned timePassed(){if(!initialized)throw std::runtime_error("fresh spline time read");return 120;}
};
struct Motion {unsigned GetCurrentMovementGeneratorType(){return 8;}};
struct Player {
    unsigned guid=1;bool inWorld=true,teleport=false;BattleGroundTG* bg=nullptr;void* map=nullptr;
    Spline* movespline=nullptr;Motion motion;
    bool IsInWorld(){return inWorld;}bool IsBeingTeleported(){return teleport;}
    unsigned GetGUIDLow(){return guid;}unsigned GetObjectGuid(){return guid;}
    unsigned GetInstanceId(){return 104;}unsigned GetUnitState(){return 0;}
    void* GetMap(){return map;}BattleGround* GetBattleGround(){return bg;}
    Motion* GetMotionMaster(){return &motion;}bool IsMoving(){return false;}
};
#include "ThornBotAdmission.inc"
struct LastMovement {
    struct Path {std::vector<int> points;auto& getPath(){return points;}}lastPath;
    unsigned failedPathRetryUntil=0;long long nextTeleport=0;
};
template<class T>struct Value {T value;T Get(){return value;}};
struct Context {LastMovement move;Value<LastMovement&> value{move};template<class T>Value<T>* GetValue(char const*){return &value;}};
struct Engine {std::string action="A: move to objective FAILED";std::string GetLastAction(){return action;}};
struct PlayerbotAI {
    Player* bot;Context context;Engine engine;unsigned reads=0;
    Player* GetBot(){return bot;}Context* GetAiObjectContext(){++reads;return &context;}
    unsigned GetState(){return 1;}Engine* GetEngine(unsigned){return &engine;}
    bool CachedActivity(ActivityType){return true;}
};
struct WorldTimer {inline static unsigned now=10000;static unsigned getMSTime(){return now;}};
struct Log {
    unsigned count=0;std::string last;
    static Log& Instance(){static Log l;return l;}
    template<class...T>void out(unsigned,char const* fmt,T...v){char b[4096];int n=std::snprintf(b,sizeof b,fmt,v...);if(n<0||n>=sizeof b)throw std::runtime_error("format overflow");last=b;++count;}
};
#include "ThornBotTrace.inc"
void Check(bool ok,char const* why){if(!ok)throw std::runtime_error(why);}
int main(){
    for(unsigned id:{30u,489u,529u,821u,999u})sMapStore.rows[id]={3};
    for(unsigned id:{0u,1u,230u})sMapStore.rows[id]={1};
    for(unsigned id:{30u,489u,529u,821u,999u})Check(WorldPosition{id}.isBg(),"stock/custom BG classification");
    for(unsigned id:{0u,1u,230u,99999u})Check(!WorldPosition{id}.isBg(),"non-BG or missing metadata");
    BattleGroundTG bg;Player p;p.bg=&bg;p.map=&bg;bg.m_Players[1]=0;PlayerbotAI ai{&p};
    auto sample=[&](){TraceThornBehavior(&ai,false);};
    sample();Check(!ai.reads,"disabled trace read context");
    bg.m_diagnostics.Configure(1,5000);sample();Check(!ai.reads,"level1 sampled bot");
    bg.m_diagnostics.Configure(2,5000);bg.status=2;sample();Check(!ai.reads,"countdown sampled");bg.status=3;
    p.map=nullptr;sample();Check(!ai.reads,"cross-map sampled");p.map=&bg;
    bg.m_Players.clear();sample();Check(!ai.reads,"nonmember sampled");bg.m_Players[1]=0;
    Spline spline;p.movespline=&spline;sample();Check(ai.reads==1,"fresh spline sample missing");
    sample();Check(ai.reads==1,"interval failed");
    bg.m_elapsed=4999;sample();Check(ai.reads==1,"early sample");
    bg.m_elapsed=5000;sample();Check(ai.reads==2,"due sample missing");
    Check(Log::Instance().last.find("actions=A:_move_to_objective_FAILED")!=std::string::npos,"action result missing");
    bg.m_elapsed+=5000;spline.initialized=true;ai.context.move.failedPathRetryUntil=10500;sample();
    Check(Log::Instance().last.find("path_retry_ms=500")!=std::string::npos,"retry not reported");
    Check(Log::Instance().last.find("spline_ms=120")!=std::string::npos,"spline not reported");
    bg.m_elapsed+=5000;bg.m_diagnostics.events=64;sample();Check(ai.reads==3,"event cap bypassed");
    bg.m_diagnostics.Advance(1000);sample();Check(ai.reads==4,"cap failed to recover");
    bg.m_botDiagnosticTicks.erase(1);p.movespline=nullptr;sample();Check(ai.reads==5,"reentry or absent spline failed");
    std::cout<<"Native map classification, trace admission and safe state formatting passed\n";
}
