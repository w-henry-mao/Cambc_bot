#pragma once
#include <cambc.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "log.h"
namespace debug {
// Set to true to measure where fuel goes: every FuelMeter adds the fuel used in its scope to a
// per-name total, and report_fuel() prints averages per turn every kFuelReportEvery turns.
// Totals are inclusive (a section's number contains the sections inside it).
constexpr bool kProfileFuel = false;
constexpr int kFuelReportEvery = 100;

struct FuelSection { const char* name = nullptr; std::uint64_t fuel = 0, calls = 0; };
inline FuelSection fuel_sections[48];
inline std::uint64_t profiled_turns = 0;

class FuelMeter {
public:
    explicit FuelMeter(const char* name) : name_(name), start_(kProfileFuel ? cambc::fuel_remaining() : 0) {}
    ~FuelMeter() {
        if (!kProfileFuel) return;
        const auto end = cambc::fuel_remaining();
        // Fuel runs out mid-turn sometimes; the refill makes the difference meaningless then.
        if (end > start_) return;
        for (auto& s : fuel_sections) {
            if (!s.name) { s.name = name_; }
            if (s.name == name_ || std::strcmp(s.name, name_) == 0) { s.fuel += start_ - end; ++s.calls; return; }
        }
    }
private:
    const char* name_;
    std::uint64_t start_;
};

// Call once at the end of each turn.
inline void report_fuel() {
    if (!kProfileFuel || ++profiled_turns % kFuelReportEvery != 0) return;
    std::cout << "FUEL " << cambc::to_string(cambc::self().champion_name()) << " over " << profiled_turns << " turns (avg per turn, calls per turn):\n";
    for (const auto& s : fuel_sections) {
        if (!s.name) break;
        std::cout << "  " << s.name << ' ' << s.fuel / profiled_turns << " (" << static_cast<double>(s.calls) / profiled_turns << ")\n";
    }
}
}
