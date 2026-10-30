#pragma once
#include "core/match_data.h"
#include "core/bot_memory.h"
#include "champions/champion_profile.h"

class Bot {
public:
    Bot();
    void play_turn();

private:
    MatchData match_data_;
    BotMemory memory_;
    const ChampionProfile* profile_;

    bool turn_guard();
};
