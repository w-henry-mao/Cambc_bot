#pragma once
// Ward placement (constants::kWardPlan, test switch): each champion keeps its one ward near a
// goal tile and moves it once it has walked away from it.
struct MatchData;
void ward_step(const MatchData& match);
