#include "bot.h"
#include "init/initializer.h"

Bot::Bot() : profile_(nullptr) {}

bool Bot::turn_guard() {
    return true;
}

void Bot::post_turn() {
}

void Bot::play_turn() {
    if (memory_.init_stage == 0) {
        run_init_stages(match_data_, memory_);
    }
}
