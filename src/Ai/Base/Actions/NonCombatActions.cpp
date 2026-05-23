/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "NonCombatActions.h"

#include "Event.h"
#include "Playerbots.h"

namespace
{
bool IsRecovering(PlayerbotAI* botAI, Player* bot, char const* auraName)
{
    if (!botAI || !bot || !auraName)
        return false;

    for (Unit::AuraApplicationMap::iterator iter = bot->GetAppliedAuras().begin(); iter != bot->GetAppliedAuras().end(); ++iter)
    {
        Aura const* aura = iter->second->GetBase();
        SpellInfo const* spellInfo = aura ? aura->GetSpellInfo() : nullptr;
        if (spellInfo && strstri(spellInfo->SpellName[0].c_str(), auraName))
            return true;
    }

    return false;
}
}

bool DrinkAction::Execute(Event event)
{
    if (IsRecovering(botAI, bot, "Drink"))
    {
        botAI->SetNextCheckDelay(2 * IN_MILLISECONDS);
        return true;
    }

    if (botAI->HasCheat(BotCheatMask::food))
    {
        // if (bot->IsNonMeleeSpellCast(true))
        //     return false;

        bot->ClearUnitState(UNIT_STATE_CHASE);
        bot->ClearUnitState(UNIT_STATE_FOLLOW);

        if (bot->isMoving())
        {
            bot->StopMoving();
            // botAI->SetNextCheckDelay(sPlayerbotAIConfig->globalCoolDown);
            // return false;
        }
        bot->SetStandState(UNIT_STAND_STATE_SIT);
        botAI->InterruptSpell();

        // float hp = bot->GetHealthPercent();
        float mp = bot->GetPowerPct(POWER_MANA);
        float p = mp;
        float delay;

        if (!bot->InBattleground())
            delay = 18000.0f * (100 - p) / 100.0f;
        else
            delay = 12000.0f * (100 - p) / 100.0f;

        botAI->SetNextCheckDelay(delay);

        bot->AddAura(25990, bot);
        return true;
        // return botAI->CastSpell(24707, bot);
    }

    return UseItemAction::Execute(event);
}

bool DrinkAction::isUseful()
{
    return !IsRecovering(botAI, bot, "Drink") && UseItemAction::isUseful() && AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < 100;
}

bool DrinkAction::isPossible()
{
    return !bot->IsInCombat() && !bot->IsMounted() &&
           !botAI->HasAnyAuraOf(GetTarget(), "dire bear form", "bear form", "cat form", "travel form", "aquatic form",
                                "flight form", "swift flight form", nullptr) &&
           (botAI->HasCheat(BotCheatMask::food) || UseItemAction::isPossible());
}

bool EatAction::Execute(Event event)
{
    if (IsRecovering(botAI, bot, "Food"))
    {
        botAI->SetNextCheckDelay(2 * IN_MILLISECONDS);
        return true;
    }

    if (botAI->HasCheat(BotCheatMask::food))
    {
        // if (bot->IsNonMeleeSpellCast(true))
        //     return false;

        bot->ClearUnitState(UNIT_STATE_CHASE);
        bot->ClearUnitState(UNIT_STATE_FOLLOW);

        if (bot->isMoving())
        {
            bot->StopMoving();
            // botAI->SetNextCheckDelay(sPlayerbotAIConfig.globalCoolDown);
            // return false;
        }

        bot->SetStandState(UNIT_STAND_STATE_SIT);
        botAI->InterruptSpell();

        float hp = bot->GetHealthPct();
        // float mp = bot->HasMana() ? bot->GetPowerPercent() : 0.f;
        float p = hp;
        float delay;

        if (!bot->InBattleground())
            delay = 18000.0f * (100 - p) / 100.0f;
        else
            delay = 12000.0f * (100 - p) / 100.0f;

        botAI->SetNextCheckDelay(delay);

        bot->AddAura(25990, bot);
        return true;
    }

    return UseItemAction::Execute(event);
}

bool EatAction::isUseful()
{
    return !IsRecovering(botAI, bot, "Food") && UseItemAction::isUseful() &&
           AI_VALUE2(uint8, "health", "self target") < 100;
}

bool EatAction::isPossible()
{
    return !bot->IsInCombat() && !bot->IsMounted() &&
           !botAI->HasAnyAuraOf(GetTarget(), "dire bear form", "bear form", "cat form", "travel form", "aquatic form",
                                "flight form", "swift flight form", nullptr) &&
           (botAI->HasCheat(BotCheatMask::food) || UseItemAction::isPossible());
}
