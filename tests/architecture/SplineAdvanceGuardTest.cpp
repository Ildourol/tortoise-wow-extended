#include <atomic>
#include <algorithm>
#include <ctime>
#include <vector>
#include <thread>
#include <stdexcept>
#include <iostream>
using int32=int;
struct Log {std::atomic<unsigned> errors{0};template<class...T>void outError(char const*,T...){++errors;}}sLog;
struct MoveSpline {
    enum UpdateResult{Result_None,Result_Arrived,Result_NextSegment};
    struct Spline {
        std::vector<int> times{0,100,200};bool cyclic=false;
        int first(){return 0;}int last(){return int(times.size())-1;}bool isCyclic(){return cyclic;}
    }spline;
    struct Flags {bool done=false;}splineflags;
    int point_Idx=0,time_passed=0;
    bool Finalized(){return splineflags.done;}
    int Duration(){return spline.times.back();}
    int next_timestamp(){return spline.times.at(point_Idx+1);}
    int segment_time_elapsed(){return next_timestamp()-time_passed;}
    UpdateResult _updateState(int32&);void _Finalize();
};
#include "SplineAdvanceNative.inc"
#include "SplineFinalizeNative.inc"
void Check(bool ok,char const* why){if(!ok)throw std::runtime_error(why);}
unsigned Advance(MoveSpline& s,int diff){unsigned steps=0;do{if(++steps>100)throw std::runtime_error("spline did not progress");s._updateState(diff);}while(diff>0);return steps;}
int main(){
    MoveSpline normal;Advance(normal,50);Check(normal.time_passed==50 && normal.point_Idx==0,"normal partial segment changed");
    Advance(normal,150);Check(normal.Finalized() && normal.time_passed==200,"normal arrival changed");
    Advance(normal,100);Check(normal.time_passed==200,"finished spline advanced");
    MoveSpline zero;zero.spline.times={0,0,100};Advance(zero,100);Check(zero.Finalized(),"zero segment stalled");
    // Independent owners hit the same exceptional log limiter concurrently.
    std::vector<std::thread> workers;std::atomic<unsigned> completed{0};
    for(unsigned i=0;i<8;++i)workers.emplace_back([&]{MoveSpline s;s.spline.times={0,100,90,200};Advance(s,200);if(s.Finalized() && s.time_passed==200)++completed;});
    for(auto& t:workers)t.join();Check(completed==8,"decreasing deadline failed to arrive");
    Check(sLog.errors==1,"parallel diagnostic limit failed");
    MoveSpline cyclic;cyclic.spline.times={0,100,90,200};cyclic.spline.cyclic=true;
    Advance(cyclic,1050);Check(!cyclic.Finalized() && cyclic.time_passed==50 && cyclic.point_Idx==0,"cycle remainder or progress changed");
    MoveSpline hitch;Advance(hitch,100000);Check(hitch.Finalized(),"large update failed to arrive");
    Check(sLog.errors==1,"repeated diagnostic not bounded");
    std::cout<<"Native spline advance/finalize and concurrent log guard passed\n";
}
