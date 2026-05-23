/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_MANUALMOVEMENTVALUE_H
#define _PLAYERBOT_MANUALMOVEMENTVALUE_H

#include "Value.h"

class PlayerbotAI;

class ManualMovementValue : public ManualSetValue<bool>
{
public:
    ManualMovementValue(PlayerbotAI* botAI, std::string const name = "manual movement")
        : ManualSetValue<bool>(botAI, false, name)
    {
    }

    std::string const Save() override { return value ? "1" : "0"; }

    bool Load(std::string const text) override
    {
        value = text == "1" || text == "true" || text == "on" || text == "yes";
        return true;
    }
};

#endif
