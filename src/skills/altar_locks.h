#pragma once
// Our own altar locks (to-do #6). A lock (10 gold, carry one) blocks enemy tributes at an altar
// until three basic attacks break it; placing it also cancels enemy tribute channels there. Our
// own tributes are not blocked. constants::kAltarLockPlan picks when we place one:
//   0 never; 1 on any unlocked altar within reach (2 tiles); 2 on an altar where a visible enemy
//   is tributing; 3 both.
void place_altar_lock();
