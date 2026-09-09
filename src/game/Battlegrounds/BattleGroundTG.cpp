#include "BattleGroundTG.h"
#include "Player.h"
#include "Creature.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "Chat.h"
#include "Config/Config.h"
#include <cmath>
#include <cstring>

namespace
{
constexpr uint32 CarrySpell = 59005, PickupSpell = 59011;
constexpr unsigned CenterObject = 12, DroppedObject = 13;
constexpr uint32 NodeLocations[4] = {161, 162, 163, 164};
constexpr uint32 GraveLocations[4] = {167, 168, 169, 170};
constexpr uint32 StartLocations[2] = {165, 166};
constexpr uint32 BannerEntries[3] = {2020400, 2020402, 2020401};
constexpr char const* NodeNames[4] = {"Amberhorn Village", "Farseer Spire", "Grimtotem Ruins", "Mage Tower"};
// Extracted Turtle WorldStateUI.dbc records 158-160, map 821.
constexpr uint32 ScoreStates[2] = {3601, 3602};
constexpr uint32 BaseStates[2] = {3621, 3622};
Team NativeTeam(ThornGorge::Team team) { return team == ThornGorge::Alliance ? ALLIANCE : team == ThornGorge::Horde ? HORDE : TEAM_NONE; }
}

BattleGroundTG::BattleGroundTG()
{
    m_BgObjects.resize(14);
    m_BgCreatures.resize(6);
    for (auto& id : m_StartMessageIds) id = 0;
    m_StartDelayTimes[BG_STARTING_EVENT_FIRST] = BG_START_DELAY_1M;
    m_StartDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_30S;
    m_StartDelayTimes[BG_STARTING_EVENT_THIRD] = BG_START_DELAY_15S;
    m_StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;
}

void BattleGroundTG::Reset()
{
    BattleGround::Reset();
    m_rules = ThornGorge::Rules{};
    m_carrier.Clear();
    m_tick = m_elapsed = 0;
    m_snapshotSequence = 0;
    m_diagnostics.Configure(sConfig.GetIntDefault("Battleground.ThornGorge.LogLevel", 0),
        sConfig.GetIntDefault("Battleground.ThornGorge.LogIntervalMs", 5000));
    for (auto& counts : m_captureCounts) counts[0] = counts[1] = 0;
}

ThornGorge::Team BattleGroundTG::Side(Player* player) const
{
    auto it = m_Players.find(player->GetObjectGuid());
    if (it == m_Players.end()) return ThornGorge::Neutral;
    Team team = it->second.PlayerTeam ? it->second.PlayerTeam : player->GetTeam();
    return team == ALLIANCE ? ThornGorge::Alliance : team == HORDE ? ThornGorge::Horde : ThornGorge::Neutral;
}

bool BattleGroundTG::Eligible(Player* player) const
{
    return player && player->IsInWorld() && player->GetMap() == GetBgMap() &&
        player->GetBattleGround() == this && Side(player) != ThornGorge::Neutral &&
        player->IsAlive() && !player->IsGameMaster() && !player->IsBeingTeleported() &&
        !player->HasAura(27827);
}

void BattleGroundTG::Announce(char const* text)
{
    for (auto const& it : m_Players)
        if (Player* player = GetBgMap()->GetPlayer(it.first))
            ChatHandler(player).PSendSysMessage("[Thorn Gorge] %s", text);
}

bool BattleGroundTG::SetupBattleGround()
{
    Trace("setup_begin", nullptr, 0, "map_assets_and_objects", true);
    for (unsigned i = 0; i < 4; ++i)
    {
        auto loc = sWorldSafeLocsStore.LookupEntry(NodeLocations[i]);
        auto grave = sWorldSafeLocsStore.LookupEntry(GraveLocations[i]);
        if (!loc || !grave || loc->map_id != 821 || grave->map_id != 821)
        { Trace("setup_failed", nullptr, i, "node_locations", true); return false; }
        for (unsigned team = 0; team < 3; ++team)
        {
            if (!AddObject(i * 3 + team, BannerEntries[team], loc->x, loc->y, loc->z, 0, 0, 0, 0, 1))
            { Trace("setup_failed", nullptr, i * 3 + team, "banner_object", true); return false; }
            // These imported templates are decorative. Capture uses nearby players.
            if (GameObject* go = GetBgMap()->GetGameObject(m_BgObjects[i * 3 + team]))
                go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            SpawnObject(m_BgObjects[i * 3 + team], RESPAWN_NEVER);
        }
    }
    for (unsigned team = 0; team < 2; ++team)
    {
        auto loc = sWorldSafeLocsStore.LookupEntry(StartLocations[team]);
        if (!loc || loc->map_id != 821 || !AddCreature(team == 0 ? 13116 : 13117, team, loc->x, loc->y, loc->z, 0))
        { Trace("setup_failed", nullptr, team, "start_spirit_guide", true); return false; }
    }
    // Objective positions are extracted DBC records. The center is provisional
    // and configurable; resolve its actual terrain/collision height, fail closed.
    m_flagX = sConfig.GetFloatDefault("Battleground.ThornGorge.FlagX", 2165.89f);
    m_flagY = sConfig.GetFloatDefault("Battleground.ThornGorge.FlagY", 1564.21f);
    m_flagZ = GetBgMap()->GetHeight(m_flagX, m_flagY, 1300.0f, true, 250.0f);
    if (!std::isfinite(m_flagZ) || m_flagZ < 1000.0f || m_flagZ > 1300.0f)
    {
        Trace("setup_failed", nullptr, 0, "center_terrain", true);
        sLog.outError("Thorn Gorge: no valid center terrain at %.2f %.2f", m_flagX, m_flagY);
        return false;
    }
    if (!AddObject(CenterObject, 2020421, m_flagX, m_flagY, m_flagZ + 0.1f, 0, 0, 0, 0, 1))
    { Trace("setup_failed", nullptr, CenterObject, "center_flag_object", true); return false; }
    SpawnObject(m_BgObjects[CenterObject], RESPAWN_NEVER);
    Trace("setup_complete", nullptr, 0, "objects_and_spirit_guides", true);
    return true;
}

void BattleGroundTG::StartingEventCloseDoors()
{
    Trace("countdown", nullptr, 60, "seconds", true);
    Announce("Match starts in 60 seconds. Capture bases by standing near their banners; deliver the central flag to a base you control. First to 1600 resources wins.");
}

void BattleGroundTG::StartingEventOpenDoors()
{
    for (unsigned node = 0; node < 4; ++node) UpdateBanner(node);
    SpawnObject(m_BgObjects[CenterObject], RESPAWN_IMMEDIATELY);
    Trace("match_start", nullptr, 0, "countdown_complete", true);
    TraceSnapshot("match_start");
    Announce("The battle has begun!");
    SendStates();
}

void BattleGroundTG::UpdateBanner(unsigned node)
{
    for (unsigned team = 0; team < 3; ++team)
        SpawnObject(m_BgObjects[node * 3 + team], m_rules.owner[node] == team ? RESPAWN_IMMEDIATELY : RESPAWN_NEVER);
    DelCreature(node + 2);
    if (m_rules.owner[node] != ThornGorge::Neutral)
    {
        auto loc = sWorldSafeLocsStore.LookupEntry(GraveLocations[node]);
        if (!AddCreature(m_rules.owner[node] == ThornGorge::Alliance ? 13116 : 13117, node + 2, loc->x, loc->y, loc->z, 0))
        { Trace("spirit_guide_failed", nullptr, node, "owned_node", true); EndNow(); }
    }
}

void BattleGroundTG::UpdateObjectives()
{
    for (unsigned node = 0; node < 4; ++node)
    {
        auto loc = sWorldSafeLocsStore.LookupEntry(NodeLocations[node]);
        unsigned counts[2] = {};
        for (auto const& it : m_Players)
            if (Player* p = GetBgMap()->GetPlayer(it.first))
                if (Eligible(p) && p->IsWithinDist3d(loc->x, loc->y, loc->z, ThornGorge::CaptureRadius) &&
                    std::abs(p->GetPositionZ() - loc->z) <= 12.0f)
                    ++counts[Side(p)];
        m_captureCounts[node][0] = counts[0];
        m_captureCounts[node][1] = counts[1];
        auto previous = m_rules.owner[node];
        m_rules.Capture(node, counts[0], counts[1]);
        if (previous != m_rules.owner[node])
        {
            Trace("node_owner_changed", nullptr, node, m_rules.owner[node] == ThornGorge::Alliance ? "alliance" : m_rules.owner[node] == ThornGorge::Horde ? "horde" : "neutral");
            UpdateBanner(node);
            if (GetStatus() != STATUS_IN_PROGRESS) return;
            std::string message = std::string(NodeNames[node]) + (m_rules.owner[node] == ThornGorge::Alliance ? " captured by Alliance." : m_rules.owner[node] == ThornGorge::Horde ? " captured by Horde." : " is neutral.");
            Announce(message.c_str());
            // Native repop chooses a still-owned graveyard. Do not revive players.
            if (previous != ThornGorge::Neutral)
                for (auto const& it : m_Players)
                    if (Player* p = GetBgMap()->GetPlayer(it.first))
                        if (p->IsDead() && p->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST) && Side(p) == previous)
                        {
                            auto grave = sWorldSafeLocsStore.LookupEntry(GraveLocations[node]);
                            if (p->IsWithinDist3d(grave->x, grave->y, grave->z, 40.0f))
                            { Trace("ghost_relocated", p, node, "base_lost"); p->RepopAtGraveyard(); }
                        }
        }
        if (Player* p = m_carrier ? GetBgMap()->GetPlayer(m_carrier) : nullptr)
            if (Eligible(p) && p->HasAura(CarrySpell) && p->IsWithinDist3d(loc->x, loc->y, loc->z, 5.0f) &&
                m_rules.Deliver(p->GetGUID(), Side(p), node))
            {
                m_carrier.Clear(); // aura removal calls EventPlayerDroppedFlag
                p->RemoveAurasDueToSpell(CarrySpell);
                auto score = m_PlayerScores.find(p->GetObjectGuid());
                if (score != m_PlayerScores.end()) ++static_cast<BattleGroundTGScore*>(score->second)->FlagCaptures;
                RewardHonorToTeam(40, NativeTeam(Side(p)));
                Trace("flag_delivered", p, node);
                Announce("Flag delivered!");
            }
    }
    for (auto const& it : m_Players)
        if (Player* p = GetBgMap()->GetPlayer(it.first))
        {
            int nearbyNode = -1;
            for (unsigned node = 0; node < 4; ++node)
            {
                auto loc = sWorldSafeLocsStore.LookupEntry(NodeLocations[node]);
                if (Eligible(p) && p->IsWithinDist3d(loc->x, loc->y, loc->z, ThornGorge::CaptureRadius))
                    nearbyNode = int(node);
            }
            UpdateWorldStateForPlayer(3623, nearbyNode >= 0, p);
            if (nearbyNode >= 0) UpdateWorldStateForPlayer(3624, m_rules.progress[nearbyNode], p);
            UpdateWorldStateForPlayer(3625, 40, p);
        }
    SendStates();
}

void BattleGroundTG::Update(uint32 diff)
{
    bool const snapshotDue = m_diagnostics.Advance(diff);
    if (GetStatus() == STATUS_WAIT_JOIN && GetPlayersSize())
    {
        // Use a start-area leash rather than guessing gate positions from video.
        for (auto const& it : m_Players)
            if (Player* p = GetBgMap()->GetPlayer(it.first))
            {
                auto side = Side(p);
                if (side == ThornGorge::Neutral || p->IsGameMaster()) continue;
                auto loc = sWorldSafeLocsStore.LookupEntry(StartLocations[side]);
                if (loc && !p->IsBeingTeleported() && !p->IsWithinDist3d(loc->x, loc->y, loc->z, 35.0f))
                    p->NearTeleportTo(loc->x, loc->y, loc->z, 0);
            }
    }
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        m_elapsed += diff;
        if (m_carrier)
        {
            Player* p = GetBgMap()->GetPlayer(m_carrier);
            if (!p || !Eligible(p) || !p->HasAura(CarrySpell))
            {
                if (p) EventPlayerDroppedFlag(p);
                else { Trace("carrier_unavailable", nullptr, m_carrier.GetCounter()); m_rules.Drop(m_rules.carrier); m_carrier.Clear(); m_rules.flag = ThornGorge::Respawning; }
            }
        }
        auto oldFlag = m_rules.flag;
        m_rules.Tick(diff);
        if (oldFlag != ThornGorge::Center && m_rules.flag == ThornGorge::Center) RestoreFlag();
        m_tick += diff;
        // Sample players once per owner update after a stall, without granting
        // retroactive capture time at their new positions.
        if (!m_rules.HasWinner() && m_tick >= 1000) { m_tick %= 1000; UpdateObjectives(); }
        if (m_rules.HasWinner() || m_elapsed >= 30 * MINUTE * IN_MILLISECONDS)
            EndBattleGround(GetWinningTeam());
    }
    if (snapshotDue && GetPlayersSize()) TraceSnapshot("periodic");
    // Native Update can delete this object. Nothing may run after it.
    BattleGround::Update(diff);
}

bool BattleGroundTG::OwnFlagObject(GameObject* object) const
{
    if (!object || object->GetMap() != GetBgMap() || !object->isSpawned()) return false;
    return (m_rules.flag == ThornGorge::Center && object->GetObjectGuid() == m_BgObjects[CenterObject]) ||
        (m_rules.flag == ThornGorge::Dropped && object->GetObjectGuid() == m_BgObjects[DroppedObject]);
}

char const* BattleGroundTG::FlagRejection(Player* player, GameObject* object)
{
    if (GetStatus() != STATUS_IN_PROGRESS) return "match_inactive";
    if (!Eligible(player)) return "player_ineligible";
    if (!player->CanUseBattleGroundObject()) return "object_use_blocked";
    if (!OwnFlagObject(object)) return "flag_unavailable_or_wrong_object";
    if (!player->IsWithinDistInMap(object, 5.0f)) return "out_of_range";
    if (!player->IsWithinLOSInMap(object)) return "no_line_of_sight";
    return nullptr;
}

void BattleGroundTG::EventPlayerClickedOnFlag(Player* player, GameObject* object)
{
    if (char const* reason = FlagRejection(player, object))
    { Trace("pickup_click_rejected", player, 0, reason); return; }
    Trace("pickup_requested", player);
    player->CastSpell(player, PickupSpell, false, nullptr, nullptr, object->GetObjectGuid());
}

void BattleGroundTG::CompleteFlagPickup(Player* player, GameObject* object)
{
    if (char const* reason = FlagRejection(player, object))
    { Trace("pickup_completion_rejected", player, 0, reason); return; }
    if (!m_rules.PickUp(player->GetGUID())) return;
    m_carrier = player->GetObjectGuid();
    SpawnObject(object->GetObjectGuid(), RESPAWN_NEVER);
    player->CastSpell(player, CarrySpell, true);
    if (!player->HasAura(CarrySpell)) { Trace("carry_aura_failed", player); EventPlayerDroppedFlag(player); return; }
    Trace("flag_picked_up", player);
    Announce("The flag has been picked up.");
    SendStates();
}

void BattleGroundTG::EventPlayerDroppedFlag(Player* player)
{
    if (!player || player->GetObjectGuid() != m_carrier) return;
    if (!m_rules.Drop(player->GetGUID())) return;
    m_carrier.Clear();
    player->RemoveAurasDueToSpell(CarrySpell);
    DelObject(DroppedObject);
    if (GetStatus() != STATUS_IN_PROGRESS || player->GetMap() != GetBgMap() ||
        !AddObject(DroppedObject, 2020421, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), 0, 0, 0, 0, 1))
        m_rules.flag = ThornGorge::Respawning;
    Trace("flag_dropped", player, 0, m_rules.flag == ThornGorge::Dropped ? "ground_flag_created" : "center_reset_scheduled");
    Announce("Flag dropped; it returns to the center after 10 seconds.");
    SendStates();
}

void BattleGroundTG::RestoreFlag()
{
    DelObject(DroppedObject);
    SpawnObject(m_BgObjects[CenterObject], RESPAWN_IMMEDIATELY);
    Trace("flag_reset");
    Announce("The flag has returned to the center.");
}

void BattleGroundTG::AddPlayer(Player* player)
{
    BattleGround::AddPlayer(player);
    m_PlayerScores[player->GetObjectGuid()] = new BattleGroundTGScore;
    Trace("player_join", player);
}

void BattleGroundTG::RemovePlayer(Player* player, ObjectGuid guid)
{
    Trace("player_leave", player, guid.GetCounter());
    if (guid == m_carrier)
    {
        if (player) EventPlayerDroppedFlag(player);
        else { Trace("carrier_unavailable", nullptr, m_carrier.GetCounter()); m_rules.Drop(m_rules.carrier); m_carrier.Clear(); m_rules.flag = ThornGorge::Respawning; }
    }
    if (player) player->RemoveAurasDueToSpell(CarrySpell);
}

void BattleGroundTG::HandleKillPlayer(Player* player, Player* killer)
{
    Trace("player_death", player, killer ? killer->GetGUIDLow() : 0);
    EventPlayerDroppedFlag(player);
    BattleGround::HandleKillPlayer(player, killer);
}

void BattleGroundTG::HandleAreaTrigger(Player*, uint32)
{
    // The map-owner timer validates position and delivery. Client trigger spam
    // must never advance capture progress or award additional points.
}

Team BattleGroundTG::GetWinningTeam() const { return NativeTeam(m_rules.Leader()); }

void BattleGroundTG::EndBattleGround(Team winner)
{
    if (GetStatus() != STATUS_IN_PROGRESS || m_rules.ended) return;
    ObjectGuid carrier = m_carrier;
    m_carrier.Clear();
    m_rules.Finish();
    if (Player* player = carrier ? GetBgMap()->GetPlayer(carrier) : nullptr) player->RemoveAurasDueToSpell(CarrySpell);
    DelObject(DroppedObject);
    SpawnObject(m_BgObjects[CenterObject], RESPAWN_NEVER);
    RewardHonorToTeam(100, ALLIANCE);
    RewardHonorToTeam(100, HORDE);
    if (winner != TEAM_NONE)
    {
        RewardHonorToTeam(200, winner);
    }
    Trace("match_end", nullptr, uint32(winner), "winner_team", true);
    TraceSnapshot("match_end");
    SendStates();
    BattleGround::EndBattleGround(winner);
}

WorldSafeLocsEntry const* BattleGroundTG::GetClosestGraveYard(Player* player)
{
    auto team = Side(player);
    if (team == ThornGorge::Neutral) team = player->GetTeam() == ALLIANCE ? ThornGorge::Alliance : ThornGorge::Horde;
    auto best = sWorldSafeLocsStore.LookupEntry(StartLocations[team]);
    float distance = best ? player->GetDistanceSqr(best->x, best->y, best->z) : 1e30f;
    if (GetStatus() == STATUS_IN_PROGRESS)
        for (unsigned node = 0; node < 4; ++node)
            if (m_rules.owner[node] == team)
                if (auto grave = sWorldSafeLocsStore.LookupEntry(GraveLocations[node]))
                {
                    float d = player->GetDistanceSqr(grave->x, grave->y, grave->z);
                    if (d < distance) { best = grave; distance = d; }
                }
    return best;
}

void BattleGroundTG::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    for (unsigned team = 0; team < 2; ++team)
    {
        FillInitialWorldState(data, count, ScoreStates[team], m_rules.score[team]);
        FillInitialWorldState(data, count, BaseStates[team], m_rules.Bases(ThornGorge::Team(team)));
    }
    FillInitialWorldState(data, count, 3603, ThornGorge::MaxScore);
    FillInitialWorldState(data, count, 3623, 0);
    FillInitialWorldState(data, count, 3624, 50);
    FillInitialWorldState(data, count, 3625, 40);
}

void BattleGroundTG::SendStates()
{
    for (unsigned team = 0; team < 2; ++team)
    {
        m_TeamScores[team] = m_rules.score[team];
        UpdateWorldState(ScoreStates[team], m_rules.score[team]);
        UpdateWorldState(BaseStates[team], m_rules.Bases(ThornGorge::Team(team)));
    }
    UpdateWorldState(3603, ThornGorge::MaxScore);
}

void BattleGroundTG::HandleCommand(Player* player, ChatHandler* handler, char* args)
{
    if (!args || !*args || !std::strcmp(args, "thorn"))
    {
        if (m_diagnostics.ManualSnapshot()) TraceSnapshot("gm_status_request");
        handler->PSendSysMessage("Thorn Gorge: Alliance %u/%u (%u bases), Horde %u/%u (%u bases), flag state %u; elapsed %us",
            m_rules.score[0], ThornGorge::MaxScore, m_rules.Bases(ThornGorge::Alliance),
            m_rules.score[1], ThornGorge::MaxScore, m_rules.Bases(ThornGorge::Horde), unsigned(m_rules.flag), m_elapsed / 1000);
        for (unsigned i = 0; i < 4; ++i) handler->PSendSysMessage("%s: progress %d, owner %u", NodeNames[i], m_rules.progress[i], unsigned(m_rules.owner[i]));
        return;
    }
    BattleGround::HandleCommand(player, handler, args);
}

ObjectGuid BattleGroundTG::GetAvailableFlag() const
{
    return m_rules.flag == ThornGorge::Center ? m_BgObjects[CenterObject] :
        m_rules.flag == ThornGorge::Dropped ? m_BgObjects[DroppedObject] : ObjectGuid();
}

bool BattleGroundTG::GetObjective(Player* player, float& x, float& y, float& z) const
{
    if (GetStatus() != STATUS_IN_PROGRESS || !Eligible(player)) return false;
    auto team = Side(player);
    float distance = 1e30f;
    bool found = false;
    for (unsigned node = 0; node < 4; ++node)
    {
        // Carriers deliver; others spread across capture points by stable GUID.
        unsigned idx = (node + player->GetGUIDLow()) % 4;
        if (m_carrier == player->GetObjectGuid() ? m_rules.owner[idx] != team : m_rules.owner[idx] == team) continue;
        auto loc = sWorldSafeLocsStore.LookupEntry(NodeLocations[idx]);
        if (!loc) continue;
        float d = player->GetDistanceSqr(loc->x, loc->y, loc->z);
        if (m_carrier != player->GetObjectGuid() || d < distance)
        { x = loc->x; y = loc->y; z = loc->z; distance = d; found = true; }
        if (m_carrier != player->GetObjectGuid()) break;
    }
    if (m_carrier != player->GetObjectGuid() && m_rules.Bases(team) && player->GetGUIDLow() % 3 == 0)
        if (GameObject* flag = GetBgMap()->GetGameObject(GetAvailableFlag()))
        { x = flag->GetPositionX(); y = flag->GetPositionY(); z = flag->GetPositionZ(); return true; }
    return found;
}


void BattleGroundTG::Trace(char const* event, Player* player, uint32 related, char const* reason, bool critical)
{
    if (!m_diagnostics.Event(critical)) return;
    sLog.out(LOG_BG, "THORN_GORGE schema=1 map=821 event=%s inst=%u elapsed_ms=%u status=%u guid=%u name=%s team=%u related=%u reason=%s score_a=%u score_h=%u bases_a=%u bases_h=%u flag=%u carrier=%u x=%.2f y=%.2f z=%.2f",
        event, GetInstanceID(), m_elapsed, uint32(GetStatus()), player ? player->GetGUIDLow() : 0,
        player ? player->GetName() : "-", player ? uint32(Side(player)) : 2, related, reason,
        m_rules.score[0], m_rules.score[1], m_rules.Bases(ThornGorge::Alliance), m_rules.Bases(ThornGorge::Horde),
        uint32(m_rules.flag), m_carrier.GetCounter(), player ? player->GetPositionX() : 0,
        player ? player->GetPositionY() : 0, player ? player->GetPositionZ() : 0);
}

void BattleGroundTG::TraceSnapshot(char const* reason)
{
    if (!m_diagnostics.level) return;
    uint32 const sequence = ++m_snapshotSequence;
    sLog.out(LOG_BG, "THORN_GORGE schema=1 map=821 event=snapshot inst=%u seq=%u elapsed_ms=%u status=%u ended=%u reason=%s score_a=%u score_h=%u bases_a=%u bases_h=%u flag=%u carrier=%u flag_timer_ms=%u players=%u suppressed_events=%u",
        GetInstanceID(), sequence, m_elapsed, uint32(GetStatus()), uint32(m_rules.ended), reason, m_rules.score[0], m_rules.score[1],
        m_rules.Bases(ThornGorge::Alliance), m_rules.Bases(ThornGorge::Horde), uint32(m_rules.flag),
        m_carrier.GetCounter(), m_rules.flagTimer, GetPlayersSize(), m_diagnostics.TakeSuppressed());
    for (unsigned node=0; node<4; ++node)
        sLog.out(LOG_BG, "THORN_GORGE schema=1 map=821 event=node inst=%u seq=%u node=%u owner=%u progress=%d nearby_a=%u nearby_h=%u",
            GetInstanceID(), sequence, node, uint32(m_rules.owner[node]), m_rules.progress[node], m_captureCounts[node][0], m_captureCounts[node][1]);
    if (m_diagnostics.level < 2) return;
    for (auto const& it : m_Players)
    {
        Player* player = GetBgMap()->GetPlayer(it.first);
        if (!player)
        {
            sLog.out(LOG_BG, "THORN_GORGE schema=1 map=821 event=player inst=%u seq=%u guid=%u available=0", GetInstanceID(), sequence, it.first.GetCounter());
            continue;
        }
        float x=0, y=0, z=0;
        bool objective=GetObjective(player,x,y,z);
        auto found=m_PlayerScores.find(it.first);
        auto* score=found==m_PlayerScores.end() ? nullptr : static_cast<BattleGroundTGScore*>(found->second);
        sLog.out(LOG_BG, "THORN_GORGE schema=1 map=821 event=player inst=%u seq=%u guid=%u name=%s available=1 team=%u socketless=%u alive=%u hp=%u max_hp=%u combat=%u gm=%u mounted=%u ghost=%u carry_aura=%u casting=%u victim=%u x=%.2f y=%.2f z=%.2f objective=%u objective_x=%.2f objective_y=%.2f objective_z=%.2f kills=%u deaths=%u honor=%u captures=%u",
            GetInstanceID(), sequence, player->GetGUIDLow(), player->GetName(), uint32(Side(player)),
            uint32(!player->GetSession() || !player->GetSession()->GetSocket()), uint32(player->IsAlive()), player->GetHealth(), player->GetMaxHealth(),
            uint32(player->IsInCombat()), uint32(player->IsGameMaster()), uint32(player->IsMounted()), uint32(player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST)),
            uint32(player->HasAura(CarrySpell)), uint32(player->IsNonMeleeSpellCasted(false)), player->GetVictim() ? player->GetVictim()->GetGUIDLow() : 0,
            player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), uint32(objective), x,y,z,
            score ? score->KillingBlows : 0, score ? score->Deaths : 0, score ? score->BonusHonor : 0, score ? score->FlagCaptures : 0);
    }
}
