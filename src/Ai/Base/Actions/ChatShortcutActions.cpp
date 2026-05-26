/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "ChatShortcutActions.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <vector>

#include "Event.h"
#include "Formations.h"
#include "LastMovementValue.h"
#include "MotionMaster.h"
#include "PlayerbotRepository.h"
#include "Playerbots.h"
#include "PositionValue.h"

namespace
{
struct HaltSnapshot
{
    std::vector<std::string> combatStrategies;
    std::vector<std::string> nonCombatStrategies;
    std::vector<std::string> deadStrategies;
    PositionInfo stayPosition;
    PositionInfo returnPosition;
};

std::map<ObjectGuid::LowType, HaltSnapshot> haltSnapshots;

std::string JoinStrategies(std::vector<std::string> const& strategies)
{
    std::ostringstream out;
    for (std::vector<std::string>::const_iterator i = strategies.begin(); i != strategies.end(); ++i)
    {
        if (i != strategies.begin())
            out << ",";
        out << "+" << *i;
    }
    return out.str();
}

void RestoreStrategies(PlayerbotAI* botAI, BotState state, std::vector<std::string> const& strategies)
{
    botAI->ClearStrategies(state);

    std::string const joined = JoinStrategies(strategies);
    if (!joined.empty())
        botAI->ChangeStrategy(joined, state);
}

void RestorePosition(PositionMap& posMap, std::string const& key, PositionInfo const& value)
{
    posMap[key] = value;
}
}

void PositionsResetAction::ResetReturnPosition()
{
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["return"];
    pos.Reset();
    posMap["return"] = pos;
}

void PositionsResetAction::SetReturnPosition(float x, float y, float z)
{
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["return"];
    pos.Set(x, y, z, botAI->GetBot()->GetMapId());
    posMap["return"] = pos;
}

void PositionsResetAction::ResetStayPosition()
{
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["stay"];
    pos.Reset();
    posMap["stay"] = pos;
}

void PositionsResetAction::SetStayPosition(float x, float y, float z)
{
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["stay"];
    pos.Set(x, y, z, botAI->GetBot()->GetMapId());
    posMap["stay"] = pos;
}

bool FollowChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    // botAI->Reset();
    botAI->ChangeStrategy("+follow,-passive,-grind,-move from group", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("-stay,-follow,-passive,-grind,-move from group", BOT_STATE_COMBAT);
    botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Reset();
    botAI->GetAiObjectContext()->GetValue<ObjectGuid>("pull target")->Set(ObjectGuid::Empty);
    context->GetValue<Unit*>("current target")->Set(nullptr);
    bot->SetTarget(ObjectGuid::Empty);
    bot->SetSelection(ObjectGuid());
    botAI->ChangeEngine(BOT_STATE_NON_COMBAT);
    bot->AttackStop();

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["return"];
    pos.Reset();
    posMap["return"] = pos;

    pos = posMap["stay"];
    pos.Reset();
    posMap["stay"] = pos;

    if (bot->IsInCombat())
    {
        Formation* formation = AI_VALUE(Formation*, "formation");
        std::string const target = formation->GetTargetName();
        bool moved = false;
        if (!target.empty())
            moved = Follow(AI_VALUE(Unit*, target));
        else
        {
            WorldLocation loc = formation->GetLocation();
            if (Formation::IsNullLocation(loc) || loc.GetMapId() == -1)
                return false;

            MovementPriority priority = botAI->GetState() == BOT_STATE_COMBAT ? MovementPriority::MOVEMENT_COMBAT : MovementPriority::MOVEMENT_NORMAL;
            moved = MoveTo(loc.GetMapId(), loc.GetPositionX(), loc.GetPositionY(), loc.GetPositionZ(), false, false, false,
                        true, priority);
        }

        if (bot->GetPet())
            botAI->PetFollow();

        if (moved)
        {
            botAI->TellMaster("Following");
            return true;
        }
    }

    /* Default mechanics takes care of this now.
    if (bot->GetMapId() != master->GetMapId() || (master && bot->GetDistance(master) >
    sPlayerbotAIConfig.sightDistance))
    {
        if (bot->isDead())
        {
            bot->ResurrectPlayer(1.0f, false);
            botAI->TellMasterNoFacing("Back from the grave!");
        }
        else
            botAI->TellMaster("You are too far away from me! I will there soon.");

        bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
        bot->TeleportTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
    master->GetOrientation()); return true;
    }
    */

    botAI->TellMaster("Following");
    return true;
}

bool StayChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("+stay,-passive,-move from group", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+stay,-follow,-passive,-move from group", BOT_STATE_COMBAT);
    bot->SetSelection(ObjectGuid());
    bot->AttackStop();

    SetReturnPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
    SetStayPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    botAI->TellMaster("Staying");
    return true;
}

bool HaltChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ObjectGuid::LowType const guid = bot->GetGUID().GetCounter();
    std::map<ObjectGuid::LowType, HaltSnapshot>::iterator snapshot = haltSnapshots.find(guid);
    if (snapshot != haltSnapshots.end())
    {
        PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();

        RestoreStrategies(botAI, BOT_STATE_COMBAT, snapshot->second.combatStrategies);
        RestoreStrategies(botAI, BOT_STATE_NON_COMBAT, snapshot->second.nonCombatStrategies);
        RestoreStrategies(botAI, BOT_STATE_DEAD, snapshot->second.deadStrategies);
        RestorePosition(posMap, "stay", snapshot->second.stayPosition);
        RestorePosition(posMap, "return", snapshot->second.returnPosition);

        haltSnapshots.erase(snapshot);
        botAI->TellMaster("Resuming");
        return true;
    }

    HaltSnapshot saved;
    saved.combatStrategies = botAI->GetStrategies(BOT_STATE_COMBAT);
    saved.nonCombatStrategies = botAI->GetStrategies(BOT_STATE_NON_COMBAT);
    saved.deadStrategies = botAI->GetStrategies(BOT_STATE_DEAD);

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    saved.stayPosition = posMap["stay"];
    saved.returnPosition = posMap["return"];
    haltSnapshots[guid] = saved;

    botAI->Reset();
    botAI->ChangeStrategy("+stay,-follow,+passive,-move from group", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+stay,-follow,+passive,-move from group", BOT_STATE_COMBAT);
    bot->SetSelection(ObjectGuid());
    bot->AttackStop();
    botAI->PetFollow();

    SetReturnPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
    SetStayPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    botAI->TellMaster("Halting");
    return true;
}

bool ManualMovementAction::Execute(Event event)
{
    std::string param = event.getParam();
    std::transform(param.begin(), param.end(), param.begin(), [](unsigned char c) { return std::tolower(c); });

    Player* target = event.getOwner();
    PlayerbotAI* targetAI = target ? GET_PLAYERBOT_AI(target) : nullptr;
    if (!targetAI || !targetAI->IsRealPlayer())
    {
        target = bot;
        targetAI = botAI;
    }

    bool& manualMovement = targetAI->GetAiObjectContext()->GetValue<bool>("manual movement")->RefGet();
    bool const oldValue = manualMovement;
    bool const commandIsOnMaster = targetAI == botAI;

    if (param.empty() || param == "?" || param == "status")
    {
        if (!commandIsOnMaster)
            return true;

        botAI->TellMaster(manualMovement ? "Selfbot combat-only is enabled for master"
                                         : "Selfbot combat-only is disabled for master");
        return true;
    }

    if (param == "on" || param == "1" || param == "enable" || param == "enabled" || param == "lock" ||
        param == "locked")
    {
        manualMovement = true;
    }
    else if (param == "off" || param == "0" || param == "disable" || param == "disabled" || param == "unlock" ||
             param == "unlocked")
    {
        manualMovement = false;
    }
    else
    {
        botAI->TellMaster("Usage: mmove on|off|?");
        return false;
    }

    if (manualMovement)
    {
        targetAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get().clear();
        target->StopMoving();
        target->ClearUnitState(UNIT_STATE_CHASE);
        target->ClearUnitState(UNIT_STATE_FOLLOW);
        if (target->GetMotionMaster())
            target->GetMotionMaster()->Clear();
    }

    if (manualMovement != oldValue)
        PlayerbotRepository::instance().Save(targetAI);

    if (commandIsOnMaster)
    {
        botAI->TellMaster(manualMovement ? "Selfbot combat-only enabled for master"
                                         : "Selfbot combat-only disabled for master");
    }
    return true;
}

bool MoveFromGroupChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    // dont need to remove stay or follow, move from group takes priority over both
    // (see their isUseful() methods)
    botAI->ChangeStrategy("+move from group", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+move from group", BOT_STATE_COMBAT);

    botAI->TellMaster("Moving away from group");
    return true;
}

bool FleeChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("+follow,-stay,+passive", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+follow,-stay,+passive", BOT_STATE_COMBAT);
    bot->SetSelection(ObjectGuid());
    bot->AttackStop();

    ResetReturnPosition();
    ResetStayPosition();

    if (bot->GetMapId() != master->GetMapId() || bot->GetDistance(master) > sPlayerbotAIConfig.sightDistance)
    {
        botAI->TellError("I will not flee with you - too far away");
        return true;
    }

    botAI->TellMaster("Fleeing");
    return true;
}

bool GoawayChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("+runaway,-stay", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+runaway,-stay", BOT_STATE_COMBAT);

    ResetReturnPosition();
    ResetStayPosition();

    botAI->TellMaster("Running away");
    return true;
}

bool GrindChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("+grind,-passive,-stay", BOT_STATE_NON_COMBAT);

    ResetReturnPosition();
    ResetStayPosition();

    botAI->TellMaster("Grinding");
    return true;
}

bool TankAttackChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    if (!botAI->IsTank(bot))
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("-passive", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("-passive", BOT_STATE_COMBAT);

    ResetReturnPosition();
    ResetStayPosition();

    botAI->TellMaster("Attacking");
    return true;
}

bool MaxDpsChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    if (!botAI->ContainsStrategy(STRATEGY_TYPE_DPS))
        return false;

    botAI->Reset();

    botAI->ChangeStrategy("-threat,-conserve mana,-cast time,+dps debuff,+boost", BOT_STATE_COMBAT);
    botAI->TellMaster("Max DPS!");

    return true;
}

bool NaxxChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("+naxx", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+naxx", BOT_STATE_COMBAT);
    botAI->TellMasterNoFacing("Add Naxx Strategies!");
    // bot->Say("Add Naxx Strategies!", LANG_UNIVERSAL);
    return true;
}

bool BwlChatShortcutAction::Execute(Event /*event*/)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    botAI->Reset();
    botAI->ChangeStrategy("+bwl", BOT_STATE_NON_COMBAT);
    botAI->ChangeStrategy("+bwl", BOT_STATE_COMBAT);
    botAI->TellMasterNoFacing("Add Bwl Strategies!");
    return true;
}
