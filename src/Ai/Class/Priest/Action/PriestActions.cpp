/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "PriestActions.h"

#include "Event.h"
#include "Playerbots.h"

namespace
{
bool IsValidPriestWandTarget(Player* bot, Unit* target)
{
    return target && target->IsAlive() && target->IsInWorld() && target->GetMapId() == bot->GetMapId() &&
        bot->IsValidAttackTarget(target);
}
}

Unit* CastPriestWandAction::GetTarget()
{
    Player* master = GetMaster();
    if (master && botAI->HasActivePlayerMaster())
    {
        if (ObjectGuid masterTargetGuid = master->GetTarget())
        {
            Unit* masterTarget = botAI->GetUnit(masterTargetGuid);
            if (IsValidPriestWandTarget(bot, masterTarget))
                return masterTarget;
        }
    }

    Unit* target = CastShootAction::GetTarget();
    if (IsValidPriestWandTarget(bot, target))
        return target;

    return nullptr;
}

bool CastPriestWandAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    context->GetValue<Unit*>("current target")->Set(target);
    bot->SetTarget(target->GetGUID());
    bot->SetSelection(target->GetGUID());
    botAI->ChangeEngine(BOT_STATE_COMBAT);

    return CastShootAction::Execute(event);
}

bool CastPriestWandAction::isUseful()
{
    Item* ranged = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!ranged || ranged->GetTemplate()->SubClass != ITEM_SUBCLASS_WEAPON_WAND)
        return false;

    return GetTarget();
}

bool CastRemoveShadowformAction::Execute(Event /*event*/)
{
    botAI->RemoveAura("shadowform");
    return true;
}

bool CastRemoveShadowformAction::isUseful() { return botAI->HasAura("shadowform", AI_VALUE(Unit*, "self target")); }

Unit* CastPowerWordShieldOnAlmostFullHealthBelowAction::GetTarget()
{
    Group* group = bot->GetGroup();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        if (player->isDead())
        {
            continue;
        }
        if (player->GetHealthPct() > sPlayerbotAIConfig.almostFullHealth)
        {
            continue;
        }
        if (player->GetDistance2d(bot) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
        {
            continue;
        }
        return player;
    }
    return nullptr;
}

bool CastPowerWordShieldOnAlmostFullHealthBelowAction::isUseful()
{
    Group* group = bot->GetGroup();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        if (player->isDead())
        {
            continue;
        }
        if (player->GetHealthPct() > sPlayerbotAIConfig.almostFullHealth)
        {
            continue;
        }
        if (player->GetDistance2d(bot) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
        {
            continue;
        }
        return true;
    }
    return false;
}

Unit* CastPowerWordShieldOnNotFullAction::GetTarget()
{
    Group* group = bot->GetGroup();
    MinValueCalculator calc(100);
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        if (player->isDead() || player->IsFullHealth())
        {
            continue;
        }
        if (player->GetDistance2d(bot) > sPlayerbotAIConfig.spellDistance)
        {
            continue;
        }
        if (botAI->HasAnyAuraOf(player, "weakened soul", "power word: shield", nullptr))
        {
            continue;
        }
        calc.probe(player->GetHealthPct(), player);
    }
    return (Unit*)calc.param;
}

bool CastPowerWordShieldOnNotFullAction::isUseful()
{
    return GetTarget();
}
