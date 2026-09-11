
#include "playerbot/playerbot.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"

#include "Database/DatabaseEnv.h"
#include "PlayerbotAI.h"

#include "Movement/TargetedMovementGenerator.h"

ServerFacade::ServerFacade() {}
ServerFacade::~ServerFacade() {}

float ServerFacade::GetDistance2d(Unit *unit, WorldObject* wo)
{
    if (!unit || !wo)
        return false;

    float dist =
#ifdef MANGOS
    unit->GetDistance2d(wo);
#endif
#ifdef CMANGOS
    sqrt(unit->GetDistance2d(wo->GetPositionX(), wo->GetPositionY(), DIST_CALC_NONE));
#endif
    return round(dist * 10.0f) / 10.0f;
}

float ServerFacade::GetDistance2d(Unit *unit, float x, float y)
{
    float dist =
#ifdef MANGOS
    unit->GetDistance2d(x, y);
#endif
#ifdef CMANGOS
    sqrt(unit->GetDistance2d(x, y, DIST_CALC_NONE));
#endif
    return round(dist * 10.0f) / 10.0f;
}

bool ServerFacade::IsDistanceLessThan(float dist1, float dist2)
{
    return dist1 - dist2 < sPlayerbotAIConfig.targetPosRecalcDistance;
}

bool ServerFacade::IsDistanceGreaterThan(float dist1, float dist2)
{
    return dist1 - dist2 > sPlayerbotAIConfig.targetPosRecalcDistance;
}

bool ServerFacade::IsDistanceGreaterOrEqualThan(float dist1, float dist2)
{
    return !IsDistanceLessThan(dist1, dist2);
}

bool ServerFacade::IsDistanceLessOrEqualThan(float dist1, float dist2)
{
    return !IsDistanceGreaterThan(dist1, dist2);
}

void ServerFacade::SetFacingTo(Unit* unit, float angle, bool force)
{
    MotionMaster &mm = *unit->GetMotionMaster();
    if (!force && !unit->IsStopped()) unit->SetFacingTo(angle);
    else
    {
        unit->SetOrientation(angle);
        unit->SendHeartBeat();
    }
    //unit->m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD));
}

bool ServerFacade::IsFriendlyTo(Unit* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsFriendlyTo(to);
#endif
#ifdef CMANGOS
    return bot->IsFriend(to);
#endif
}

bool ServerFacade::IsHostileTo(Unit* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsHostileTo(to);
#endif
#ifdef CMANGOS
    return bot->IsEnemy(to);
#endif
}

bool ServerFacade::IsFriendlyTo(WorldObject* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsFriendlyTo(to);
#endif
#ifdef CMANGOS
    return bot->IsFriend(to);
#endif
}

bool ServerFacade::IsHostileTo(WorldObject* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsHostileTo(to);
#endif
#ifdef CMANGOS
    return bot->IsEnemy(to);
#endif
}


bool ServerFacade::IsSpellReady(Player* bot, uint32 spell, uint32 itemId)
{
#ifdef MANGOS
    return !bot->HasSpellCooldown(spell);
#endif
#ifdef CMANGOS
    if (itemId)
    {
        const ItemPrototype* proto = sObjectMgr.GetItemPrototype(itemId);
        return bot->IsSpellReady(spell, proto);
    }
    else
        return bot->IsSpellReady(spell);
#endif
}



bool ServerFacade::IsUnderwater(Unit *unit)
{
#ifdef MANGOS
    return unit->IsUnderWater();
#endif
#ifdef CMANGOS
    return unit->IsUnderwater();
#endif
}

FactionTemplateEntry const* ServerFacade::GetFactionTemplateEntry(Unit *unit)
{
#ifdef MANGOS
    return unit->getFactionTemplateEntry();
#endif
#ifdef CMANGOS
    return unit->GetFactionTemplateEntry();
#endif
}

Unit* ServerFacade::GetChaseTarget(Unit* target)
{
    if (!target)
        return nullptr;

    MotionMaster* mm = target->GetMotionMaster();
    if (!mm || !mm->GetCurrent())
        return nullptr;

    MovementGenerator const* movement = mm->GetCurrent();
    MovementGeneratorType type = movement->GetMovementGeneratorType();

    if (target->IsPlayer())
    {
        if (type == CHASE_MOTION_TYPE)
            return static_cast<ChaseMovementGenerator<Player> const*>(movement)->GetTarget();

        if (type == FOLLOW_MOTION_TYPE)
            return static_cast<FollowMovementGenerator<Player> const*>(movement)->GetTarget();
    }
    else
    {
        if (type == CHASE_MOTION_TYPE)
            return static_cast<ChaseMovementGenerator<Creature> const*>(movement)->GetTarget();

        if (type == FOLLOW_MOTION_TYPE)
            return static_cast<FollowMovementGenerator<Creature> const*>(movement)->GetTarget();
    }

    return nullptr;
}

float ServerFacade::GetChaseAngle(Unit* target)
{
    if (!target)
        return 0.0f;

    MotionMaster* mm = target->GetMotionMaster();
    if (!mm || !mm->GetCurrent())
        return 0.0f;

    MovementGenerator const* movement = mm->GetCurrent();
    MovementGeneratorType type = movement->GetMovementGeneratorType();

    if (target->IsPlayer())
    {
        if (type == CHASE_MOTION_TYPE)
            return static_cast<ChaseMovementGenerator<Player> const*>(movement)->GetAngle();

        if (type == FOLLOW_MOTION_TYPE)
            return static_cast<FollowMovementGenerator<Player> const*>(movement)->GetAngle();
    }
    else
    {
        if (type == CHASE_MOTION_TYPE)
            return static_cast<ChaseMovementGenerator<Creature> const*>(movement)->GetAngle();

        if (type == FOLLOW_MOTION_TYPE)
            return static_cast<FollowMovementGenerator<Creature> const*>(movement)->GetAngle();
    }

    return 0.0f;
}

float ServerFacade::GetChaseOffset(Unit* target)
{
    if (!target)
        return 0.0f;

    MotionMaster* mm = target->GetMotionMaster();
    if (!mm || !mm->GetCurrent())
        return 0.0f;

    MovementGenerator const* movement = mm->GetCurrent();
    MovementGeneratorType type = movement->GetMovementGeneratorType();

    if (target->IsPlayer())
    {
        if (type == CHASE_MOTION_TYPE)
            return static_cast<ChaseMovementGenerator<Player> const*>(movement)->GetOffset();

        if (type == FOLLOW_MOTION_TYPE)
            return static_cast<FollowMovementGenerator<Player> const*>(movement)->GetOffset();
    }
    else
    {
        if (type == CHASE_MOTION_TYPE)
            return static_cast<ChaseMovementGenerator<Creature> const*>(movement)->GetOffset();

        if (type == FOLLOW_MOTION_TYPE)
            return static_cast<FollowMovementGenerator<Creature> const*>(movement)->GetOffset();
    }

    return 0.0f;
}

bool ServerFacade::isMoving(Unit *unit)
{
#ifdef MANGOS
    return unit->m_movementInfo.HasMovementFlag(movementFlagsMask);
#endif
#ifdef CMANGOS
#ifdef MANGOSBOT_ONE
    return !unit->IsStopped() || unit->IsFalling() || unit->IsJumping();
#else
    return !unit->IsStopped() || unit->IsFalling();
#endif
#endif
}
