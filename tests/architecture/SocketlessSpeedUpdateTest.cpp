#include <iostream>
#include <stdexcept>
#include <vector>
enum UnitMoveType { MOVE_RUN, MOVE_SWIM, MAX_MOVE_TYPE };
using MovementChangeType=UnitMoveType;
struct Session { bool socket=true,connected=true; void* GetSocket(){return socket?this:nullptr;} bool IsConnected(){return connected;} };
struct Player { Session* session; Session* GetSession(){return session;} };
struct Unit {
    Player* controller=nullptr; bool moved=true,inWorld=true;
    float m_speed_rate[MAX_MOVE_TYPE]={1,1};
    std::vector<std::pair<UnitMoveType,float>> pending;
    unsigned broadcasts=0;
    bool IsMovedByPlayer(){return moved;}
    Player* GetPlayerMovingMe(){return controller;}
    bool IsInWorld(){return inWorld;}
    bool HasPendingMovementChange(MovementChangeType t){for(auto p:pending)if(p.first==t)return true;return false;}
    void SetSpeedRateReal(UnitMoveType t,float v){m_speed_rate[t]=v;}
    void SetSpeedRate(UnitMoveType,float);
    void Timeout(){for(auto p:pending)SetSpeedRateReal(p.first,p.second);pending.clear();}
};
namespace MovementPacketSender {
    MovementChangeType GetChangeTypeByMoveType(UnitMoveType t){return t;}
    void AddSpeedChangeToController(Unit* u,UnitMoveType t,float r){u->pending.emplace_back(t,r);}
    void SendSpeedChangeToAll(Unit* u,UnitMoveType,float){++u->broadcasts;}
}
#include "SocketlessSpeedNative.inc"
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
int main(){
    Session synthetic;synthetic.socket=false;Player bot{&synthetic};Unit u;u.controller=&bot;
    u.SetSpeedRate(MOVE_RUN,2); // mount while stationary, synthetic client mover is set
    Check(u.m_speed_rate[MOVE_RUN]==2&&u.pending.empty(),"socketless mount waited for nonexistent ACK");
    u.moved=false;u.SetSpeedRate(MOVE_RUN,1); // dismount during a server spline
    u.Timeout();
    Check(u.m_speed_rate[MOVE_RUN]==1&&u.broadcasts==2,"stale mount speed replayed after dismount");
    u.moved=true;u.SetSpeedRate(MOVE_RUN,1.25f);u.SetSpeedRate(MOVE_SWIM,1.5f);
    Check(u.m_speed_rate[MOVE_RUN]==1.25f&&u.m_speed_rate[MOVE_SWIM]==1.5f,"legitimate speed aura lost");
    Session live;Player human{&live};Unit h;h.controller=&human;
    h.SetSpeedRate(MOVE_RUN,2);
    Check(h.pending.size()==1&&h.m_speed_rate[MOVE_RUN]==1&&!h.broadcasts,"human ACK contract bypassed");
    h.Timeout();Check(h.m_speed_rate[MOVE_RUN]==2,"human acknowledgement did not apply");
    Unit possessed;possessed.controller=&human;possessed.SetSpeedRate(MOVE_RUN,1.6f);
    Check(possessed.pending.size()==1,"human possession lost ACK contract");
    Unit loading;loading.controller=&human;loading.inWorld=false;loading.SetSpeedRate(MOVE_RUN,2);
    Check(loading.pending.empty()&&loading.m_speed_rate[MOVE_RUN]==2&&!loading.broadcasts,"pre-world contract changed");
    live.connected=false;Unit disconnected;disconnected.controller=&human;disconnected.SetSpeedRate(MOVE_RUN,0.5f);
    Check(disconnected.pending.empty()&&disconnected.m_speed_rate[MOVE_RUN]==0.5f,"disconnected controller waits for ACK");
    Unit creature;creature.moved=false;creature.SetSpeedRate(MOVE_RUN,1.6f);
    Check(creature.m_speed_rate[MOVE_RUN]==1.6f&&creature.broadcasts==1,"server creature update changed");
    std::cout<<"Socketless mount/dismount ordering, aura speeds, human ACK, possession and pre-world contracts passed\n";
}
