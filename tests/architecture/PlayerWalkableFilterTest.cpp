#include <stdexcept>
#include <string>
#include <iostream>
using uint32=unsigned;
constexpr unsigned NAV_GROUND=1,NAV_WATER=8,NAV_MAGMA=2,NAV_SLIME=4,NAV_STEEP_SLOPES=16;
constexpr unsigned TYPEID_PLAYER=4,TYPEID_UNIT=3,UNIT_STAT_IGNORE_PATHFINDING=1;
struct Unit {unsigned type=4,map=821;bool walk=true,swim=true,fly=false,ignore=false;bool CanFly()const{return fly;}bool HasUnitState(unsigned)const{return ignore;}bool CanWalk()const{return walk;}bool CanSwim()const{return swim;}unsigned GetTypeId()const{return type;}unsigned GetMapId()const{return map;}};
struct Config {bool enabled=true;bool GetBoolDefault(char const* key,bool fallback){return std::string(key)=="mmap.PlayerWalkable"?enabled:fallback;}} sConfig;
struct dtQueryFilter {unsigned include=0,exclude=0;void setIncludeFlags(unsigned f){include=f;}void setExcludeFlags(unsigned f){exclude=f;}unsigned getIncludeFlags()const{return include;}unsigned getExcludeFlags()const{return exclude;}};
using dtPolyRef=unsigned;constexpr unsigned INVALID_POLYREF=0,VERTEX_SIZE=3;
float dtVdist(float const*,float const*){return 0;}
struct PathInfo {
 Unit const* m_sourceUnit;dtQueryFilter m_filter;void* m_navMeshQuery=nullptr;static unsigned seenInclude,seenExclude;
 void createFilter();void updateFilter(){}
 static dtPolyRef FindWalkPoly(void*,float const*,dtQueryFilter const& f,float*){seenInclude=f.include;seenExclude=f.exclude;return 1;}
 dtPolyRef getPolyByLocation(float const*,float*,uint32 allowedFlags=0);
};
unsigned PathInfo::seenInclude=0,PathInfo::seenExclude=0;
#include "PlayerWalkableFilterNative.inc"
#include "PlayerWalkableLookupNative.inc"
void Check(bool b){if(!b)throw std::runtime_error("native player slope filter regression");}
int main(){
 Unit u;PathInfo p{&u};p.createFilter();Check(p.m_filter.include==(NAV_GROUND|NAV_WATER));Check(p.m_filter.exclude==16);
 float pos[3]={},distance=0;p.getPolyByLocation(pos,&distance,NAV_MAGMA);Check(p.seenExclude==16&&p.seenInclude==(NAV_GROUND|NAV_WATER|NAV_MAGMA));
 for(unsigned map:{0u,1u,30u,489u,529u,33u,409u}){u.map=map;p.createFilter();Check(p.m_filter.exclude==16);}u.map=821;
 u.fly=true;p.createFilter();Check(p.m_filter.exclude==0);u.fly=false;u.ignore=true;p.createFilter();Check(p.m_filter.exclude==0);u.ignore=false;
 u.type=TYPEID_UNIT;p.createFilter();Check(p.m_filter.exclude==0&&p.m_filter.include==15);u.type=TYPEID_PLAYER;
 sConfig.enabled=false;p.createFilter();Check(p.m_filter.exclude==0);sConfig.enabled=true;
 u.swim=false;p.createFilter();Check(p.m_filter.include==NAV_GROUND&&p.m_filter.exclude==16);
 p.m_sourceUnit=nullptr;p.createFilter();Check(p.m_filter.include==9&&p.m_filter.exclude==0);
 std::cout<<"Native generic player slope filter, all map types, creature swimming and polygon lookup passed\n";
}
