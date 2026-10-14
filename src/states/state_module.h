#pragma once
#include <cambc.h>
#include "../brain/state.h"
#include "../core/bot_memory.h"
struct TurnContext;
enum class StateResult { Continue, Done, Failed };
struct StateModule {
    virtual ~StateModule() = default;
    virtual int score(const TurnContext& ctx) = 0;
    virtual StateResult execute(TurnContext& ctx, BotMemory& memory) = 0;
    virtual void on_enter(TurnContext& ctx, BotMemory& memory) {}
    virtual void on_exit(TurnContext& ctx, BotMemory& memory) {}
    virtual int switch_margin() const { return 10; }
};
