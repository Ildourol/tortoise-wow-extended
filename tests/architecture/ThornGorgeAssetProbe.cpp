#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstdint>
static void Check(bool ok, char const* msg) { if (!ok) throw std::runtime_error(msg); }
int main(int argc, char** argv)
{
    Check(argc == 2, "usage: ThornGorgeAssetProbe <map-821-mmaps-directory>");
    std::filesystem::path dir(argv[1]);
    dtNavMeshParams params{};
    std::ifstream root(dir/"821.mmap", std::ios::binary);
    Check(bool(root.read(reinterpret_cast<char*>(&params), sizeof(params))), "invalid map parameters");
    dtNavMesh mesh; Check(dtStatusSucceed(mesh.init(&params)), "mesh init failed");
    unsigned tiles=0;
    for (auto const& entry: std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() != ".mmtile" || entry.path().filename().string().rfind("821",0)!=0) continue;
        std::ifstream file(entry.path(), std::ios::binary);
        std::uint32_t h[5]{};
        Check(bool(file.read(reinterpret_cast<char*>(h),sizeof(h))) && h[0]==0x4d4d4150 && h[1]==DT_NAVMESH_VERSION && h[2]==6 && h[3]<64000000, "tile version/header mismatch");
        auto* bytes=static_cast<unsigned char*>(dtAlloc(h[3],DT_ALLOC_PERM));
        Check(bytes && bool(file.read(reinterpret_cast<char*>(bytes),h[3])), "truncated tile");
        auto result=mesh.addTile(bytes,h[3],DT_TILE_FREE_DATA,0,nullptr);
        if(dtStatusFailed(result)) dtFree(bytes);
        Check(dtStatusSucceed(result), "tile load failed"); ++tiles;
    }
    dtNavMeshQuery query; Check(dtStatusSucceed(query.init(&mesh,65535)), "query init failed");
    // WoW XYZ converted to Detour YZX, matching native PathFinder.
    float world[][3]={{2043.89f,1729.70f,1190.03f},{2048.46f,1393.48f,1194.51f},
        {2286.36f,1402.54f,1197.30f},{2284.85f,1731.11f,1190.06f},
        {2554.20f,1597.33f,1273.19f},{1806.60f,1538.97f,1263.27f},
        {2174.469482f,1569.349243f,1160.459473f}};
    char const* names[]={"Amberhorn","Farseer","Grimtotem","MageTower","AllianceStart","HordeStart","Center"};
    dtPolyRef refs[7]{}; float points[7][3]{}; dtQueryFilter filter;
    filter.setIncludeFlags(0x09); // native ground + water, excludes steep/lava
    float ext[3]={5,150,5};
    std::cout<<"Loaded "<<tiles<<" map 821 tiles\n";
    for(unsigned i=0;i<7;++i)
    {
        float p[3]={world[i][1],world[i][2],world[i][0]};
        auto result=query.findNearestPoly(p,ext,&filter,&refs[i],points[i]);
        Check(dtStatusSucceed(result)&&refs[i], "objective has no nearby navigable polygon");
        std::cout<<names[i]<<" snapped XYZ "<<points[i][2]<<","<<points[i][0]<<","<<points[i][1]
            <<" deltaZ "<<points[i][1]-world[i][2]<<"\n";
    }
    unsigned complete=0,partial=0;
    for(unsigned from=0;from<7;++from) for(unsigned to=0;to<7;++to)
    {
        if(from==to) continue;
        dtPolyRef path[8192]; int count=0;
        auto result=query.findPath(refs[from],refs[to],points[from],points[to],&filter,path,&count,8192);
        bool full=dtStatusSucceed(result)&&count&&path[count-1]==refs[to];
        if(full) ++complete; else {++partial; std::cout<<"INCOMPLETE "<<names[from]<<" -> "<<names[to]<<"\n";}
    }
    std::cout<<"Routes: "<<complete<<" complete, "<<partial<<" incomplete (incomplete routes require explicit traversal review)\n";
    return partial ? 2 : 0;
}
