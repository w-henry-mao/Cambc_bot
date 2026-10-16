#pragma once
// Test-build instrumentation for the fight calculator (off unless constants::kCombatTrace).
// Logs FIGHT_PRED when a fight starts and FIGHT_END when it finishes; tools/accuracy_report.py
// compares them. combat_trace_turn_start() predicts, so it runs before we act (the model's
// turn 0 is our current turn); combat_trace_turn() runs at the end of every turn.
void combat_trace_turn_start();
void combat_trace_turn();
// True once this champion's traced fight has ended (duel test builds resign then).
bool combat_trace_fight_finished();
