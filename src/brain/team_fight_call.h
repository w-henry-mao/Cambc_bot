#pragma once
// When to call a team fight. Runs once per turn before the state selector; a call made here is
// followed by every teammate's attack state (states/attack.cpp).
void decide_team_fight_call();
