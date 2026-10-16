#pragma once
// Test-build duel mode (constants::kDuelMode): returns true if it handled this turn, in which
// case the normal flow is skipped. With duel mode off it always returns false.
bool duel_mode_turn();
// Duel mode only: resign once our fighter's duel is over (or very late), to end test matches early.
void duel_mode_end_of_turn();
