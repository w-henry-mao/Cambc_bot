#include <cambc.h>
#include <iostream>
#include "bot.h"
#include "draft/draft.h"
#include "debug/log.h"

int main() {
    Bot bot;
    while (true) {
        try {
            if constexpr (debug::kLogging) {
                std::cout << "TURN START fuel=" << cambc::fuel_remaining() << " phase=" << (cambc::is_draft() ? "draft" : "play") << '\n';
            }
            if (cambc::is_draft()) play_draft();
            else bot.play_turn();
            if constexpr (debug::kLogging) {
                std::cout << "TURN END fuel=" << cambc::fuel_remaining() << '\n';
            }
        } catch (const std::exception& error) {
            std::cout << "bot error: " << error.what() << std::endl;
        } catch (...) {
            std::cout << "bot error: unknown exception" << std::endl;
        }
        cambc::end_turn();
    }
}
