#include <vector>
#include <stdexcept>
#include <iostream>
using uint32=unsigned;
constexpr unsigned HIGHGUID_GAMEOBJECT=1,GO_ANIMPROGRESS_DEFAULT=100,GO_STATE_READY=0;
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
struct Map;
struct GameObject {
 inline static bool succeeds=true;inline static int live=0;
 float scale=1.25f,modelScale=1.25f;bool published=false;
 GameObject(){++live;}~GameObject(){--live;}
 bool Create(unsigned,unsigned,Map*,float,float,float,float,float,float,float,float,unsigned,unsigned){return succeeds;}
 void SetObjectScale(float v){Check(!published,"scale changed after visibility");scale=v;}
 void UpdateModel(){Check(!published,"model rebuilt after visibility");modelScale=scale;}
 unsigned GetObjectGuid(){return 99;}
};
struct Map{
 GameObject* object=nullptr;
 unsigned GenerateLocalLowGuid(unsigned){return 1;}
 void Add(GameObject* go){Check(go->modelScale==go->scale,"collision differs from visible scale");object=go;go->published=true;}
 ~Map(){delete object;}
};
struct Log{template<class...A>void outErrorDb(A...){}template<class...A>void outError(A...){}}sLog;
struct BattleGround {
 Map* map=nullptr;std::vector<unsigned> m_BgObjects=std::vector<unsigned>(15);
 Map* GetBgMap(){return map;}
 bool AddObject(uint32,uint32,float,float,float,float,float,float,float,float,float=0);
};
#include "BattlegroundObjectScaleNative.inc"
int main(){
 for(float scale:{0.0f,2.5f,0.5f}){
  Map map;BattleGround bg;bg.map=&map;
  Check(bg.AddObject(14,2020408,1,2,3,0,0,0,0,1,scale),"creation failed");
  Check(map.object->scale==(scale>0?scale:1.25f)&&map.object->published&&bg.m_BgObjects[14]==99,"scale/native publication contract");
 }
 Check(GameObject::live==0,"object leak");
 BattleGround bg;Check(!bg.AddObject(0,1,1,2,3,0,0,0,0,1),"missing map accepted");
 Map map;bg.map=&map;GameObject::succeeds=false;Check(!bg.AddObject(0,1,1,2,3,0,0,0,0,1,2.5f)&&!map.object&&GameObject::live==0,"failed create leaked/published");
 std::cout<<"Native initial scale, collision, default and failure contracts passed\n";
}
