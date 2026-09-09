#include <stdexcept>
#include <iostream>
struct Unit{};Unit* current=nullptr;float targetDistance=0;
#define AI_VALUE(type,key) current
#define AI_VALUE2(type,key,qualifier) targetDistance
struct Facade{bool IsDistanceLessThan(float a,float b){return a<b;}bool IsDistanceGreaterThan(float a,float b){return a>b;}}sServerFacade;
float GetAttackDistance(){return 10;}
bool Decision(bool hasEnemy){bool canAttackTarget=false,shouldChaseTarget=false,farFromTarget=false;
#include "MountTargetDecisionNative.inc"
return canAttackTarget;}
int main(){Unit enemy;if(Decision(true))throw std::runtime_error("missing current target blocks mounting");current=&enemy;targetDistance=5;if(!Decision(true))throw std::runtime_error("near target should dismount");targetDistance=50;if(Decision(true)||Decision(false))throw std::runtime_error("distant/no enemy blocks mounting");std::cout<<"Absent, nearby, distant and no-enemy mounting target decisions passed\n";}
