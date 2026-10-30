#include <cambc.h>
#include "bot.h"
#include "draft/draft.h"

int main() {
    Bot bot;
    while (true) {
        try {
            if (cambc::is_draft()) play_draft();
            else bot.play_turn();
        } catch (...) {
            // Always reach end_turn so one bot error does not stop the match loop.
        }
        cambc::end_turn();
    }
}
