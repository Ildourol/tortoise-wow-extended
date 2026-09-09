// Execute the production packet writers, then decode their wire coordinates.
// Deterministic spline timestamps model a winding, constant-speed ground route.
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>
using uint32=uint32_t;using int32=int32_t;
struct Vector3 {float x=0,y=0,z=0;Vector3 operator-(Vector3 b)const{return{x-b.x,y-b.y,z-b.z};}};
struct ByteBuffer {
    std::vector<unsigned char> bytes;
    size_t wpos()const{return bytes.size();}
    template<class T>ByteBuffer& operator<<(T v){append(&v,1);return *this;}
    template<class T>void append(T const* p,size_t n){auto b=reinterpret_cast<unsigned char const*>(p);bytes.insert(bytes.end(),b,b+n*sizeof(T));}
    template<class T>void put(size_t off,T v){if(off+sizeof(T)>bytes.size())throw std::runtime_error("put overflow");std::memcpy(bytes.data()+off,&v,sizeof v);}
    template<class T>T read(size_t off)const{T v;if(off+sizeof(T)>bytes.size())throw std::runtime_error("read overflow");std::memcpy(&v,bytes.data()+off,sizeof v);return v;}
    void appendPackXYZ(float x,float y,float z){*this<<uint32((int32(x/0.25f)&0x7ff)|((int32(y/0.25f)&0x7ff)<<11)|((int32(z/0.25f)&0x3ff)<<22));}
};
using WorldPacket=ByteBuffer;
constexpr unsigned CONFIG_UINT32_MAX_POINTS_PER_MVT_PACKET=0;
struct World {unsigned cap=5;unsigned getConfig(unsigned){return cap;}}sWorld;
template<class T>struct Spline {
    std::vector<Vector3> points;
    unsigned getPointCount()const{return unsigned(points.size());}
    Vector3 const& getPoint(unsigned i)const{return points.at(i);}
};
struct MoveSplineFlag {static constexpr unsigned Mask_CatmullRom=1;bool cyclic=false;unsigned value=0;operator unsigned()const{return value;}};
struct MoveSpline {
    Spline<int32> spline;MoveSplineFlag splineflags;int time_passed=0,index=1;
    std::vector<int> times;Vector3 position;
    int Duration()const{return times.back();}
    int Duration(int a,int b)const{return times.at(b)-times.at(a);}
    unsigned CountSplinePoints()const{return unsigned(times.size()-1);}
    int timePassed()const{return time_passed;}
    int _currentSplineIdx()const{return index;}
    Vector3 ComputePosition()const{return position;}
};
struct PacketBuilder {
    static void WriteCommonMonsterMovePart(MoveSpline const& m,WorldPacket& p){p<<m.spline.getPoint(1)<<uint32(77)<<uint8_t(0)<<uint32(256)<<m.Duration();}
    static int WriteMonsterMove(MoveSpline const&,WorldPacket&,int firstPoint=1);
};
#include "SplinePacketNative.inc"
void Check(bool ok,char const* msg){if(!ok)throw std::runtime_error(msg);}
bool Near(Vector3 a,Vector3 b){return std::fabs(a.x-b.x)<.26f&&std::fabs(a.y-b.y)<.26f&&std::fabs(a.z-b.z)<.26f;}
std::vector<Vector3> Decode(WorldPacket const& p){
    unsigned n=p.read<uint32>(25);Vector3 dest=p.read<Vector3>(29);std::vector<Vector3> out;
    for(unsigned i=1;i<n;++i){auto v=p.read<uint32>(41+(i-1)*4);auto sign=[](unsigned a,unsigned bits){return (a&(1u<<(bits-1)))?int(a)-int(1u<<bits):int(a);};
        out.push_back(dest-Vector3{sign(v&2047,11)*.25f,sign((v>>11)&2047,11)*.25f,sign(v>>22,10)*.25f});}
    out.push_back(dest);Check(p.bytes.size()==41+(n-1)*4,"packet node count does not match bytes");return out;
}
void Run(){
    MoveSpline m;m.spline.points.push_back({});m.times={0,0};
    for(int i=0;i<16;++i){m.spline.points.push_back({float(i*3),float((i%4)*4),float(1160+i)});if(i)m.times.push_back(i*1000);}
    m.spline.points.push_back(m.spline.points.back());
    m.position=m.spline.points[1];WorldPacket initial;int last=PacketBuilder::WriteMonsterMove(m,initial);
    Check(last==6,"initial chunk cap changed");
    // Reach a turn before the chunk endpoint: the remaining two vertices must
    // survive continuation, with current position and remaining duration.
    m.index=5;m.time_passed=4500;m.position={13.5f,2,1164.5f};WorldPacket next;
    int nextLast=PacketBuilder::WriteMonsterMove(m,next,last+1);auto path=Decode(next);
    Check(Near(next.read<Vector3>(0),m.position),"continuation repeats original route origin");
    Check(Near(path.front(),m.spline.points[m.index+1]),"continuation skips untraversed turn");
    Check(nextLast==10,"continuation cap must start at current segment");
    Check(next.read<uint32>(21)==5500,"continuation duration must end at emitted destination");
    Check(initial.read<uint32>(21)==6000,"initial partial duration is one segment short");
    // A delayed world tick can already be beyond the old chunk boundary.
    m.index=12;m.time_passed=11500;m.position={34.5f,10,1171.5f};WorldPacket final;
    Check(PacketBuilder::WriteMonsterMove(m,final,nextLast+1)==-1,"final chunk not final");auto tail=Decode(final);
    Check(Near(tail.front(),m.spline.points[13]),"late continuation includes points behind player");
    Check(final.read<uint32>(21)==3500,"final duration mismatch");
    sWorld.cap=80;m.time_passed=0;m.index=1;m.position=m.spline.points[1];WorldPacket shortPath;
    Check(PacketBuilder::WriteMonsterMove(m,shortPath)==-1,"short path split");
    Check(Decode(shortPath).size()==15&&shortPath.read<uint32>(21)==15000,"short route changed");
    m.splineflags.value=1;WorldPacket smooth;Check(PacketBuilder::WriteMonsterMove(m,smooth)==-1,"smooth split");
    Check(smooth.read<uint32>(25)==15,"smooth node count changed");
    std::cout<<"Native ground spline packet origin, corners, duration, late ticks and smooth route checks passed\n";
}

int main(){try{Run();return 0;}catch(std::exception const& e){std::cerr<<e.what()<<"\n";return 1;}}
