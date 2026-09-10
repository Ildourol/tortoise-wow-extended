#include <iostream>
#include <stdexcept>
#include <string>
struct Event{};namespace BotState{constexpr int BOT_STATE_COMBAT=1;}
struct Player{bool mounted=false,combat=false,transport=false,taxi=false,flying=false,falling=false,casting=false;bool IsMounted(){return mounted;}bool IsInCombat(){return combat;}bool GetTransport(){return transport;}bool IsTaxiFlying(){return taxi;}bool IsFlying(){return flying;}bool IsFalling(){return falling;}bool IsNonMeleeSpellCasted(bool){return casting;}};
struct Action{unsigned duration=3000;unsigned GetDuration(){return duration;}};
struct Context{Action mount;Action* GetAction(char const*){return &mount;}};
struct AI{bool combat=false,jumping=false,result=true;unsigned attempts=0;Context context;bool IsStateActive(int){return combat;}bool IsJumping(){return jumping;}bool DoSpecificAction(char const* name,Event,bool silent){if(std::string(name)!="check mount state"||!silent)throw std::runtime_error("native mount dispatch changed");++attempts;return result;}Context* GetAiObjectContext(){return &context;}};
struct MovementAction{Player* bot;AI* ai;unsigned duration=0;void SetDuration(unsigned value){duration=value;}bool TryMountForTravel(float,bool,bool,bool);};
#include "TravelMountPreparationNative.inc"
void Check(bool b,char const* m){if(!b)throw std::runtime_error(m);}
int main(){Player p;AI ai;MovementAction move{&p,&ai};
 Check(move.TryMountForTravel(80,false,false,false)&&ai.attempts==1&&move.duration==3000,"native mount/cast duration lost");
 for(bool* guard:{&p.mounted,&p.combat,&p.transport,&p.taxi,&p.flying,&p.falling,&p.casting,&ai.combat,&ai.jumping}){*guard=true;unsigned before=ai.attempts;Check(!move.TryMountForTravel(80,false,false,false)&&ai.attempts==before,"invalid travel state attempted mount");*guard=false;}
 Check(!move.TryMountForTravel(40,false,false,false),"short movement interrupted");
 Check(!move.TryMountForTravel(80,true,false,false),"idle movement interrupted");Check(!move.TryMountForTravel(80,false,true,false),"reaction movement interrupted");Check(!move.TryMountForTravel(80,false,false,true),"explicit direct movement interrupted");
 ai.result=false;move.duration=1500;Check(!move.TryMountForTravel(80,false,false,false)&&move.duration==1500,"native refusal blocked travel/changed delay");
 ai.result=true;ai.context.mount.duration=1500;Check(move.TryMountForTravel(80,false,false,false)&&move.duration==1500,"mount cast duration hardcoded");
 std::cout<<"Shared travel mount preparation preserves native eligibility, guards and cast duration\n";
}
