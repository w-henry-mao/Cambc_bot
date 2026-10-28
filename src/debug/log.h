#pragma once
#include <iostream>
#include <cambc.h>
namespace debug {
constexpr bool kEnableLogging = true;
constexpr bool kLogFuel = false;
constexpr bool kLogPathfinding = false;
constexpr bool kLogCombat = false;
constexpr bool kLogFarm = false;
constexpr bool kLogDraft = true;
constexpr bool kLogInit = true;

inline std::string get_champion_name() {
    if (cambc::is_draft()) {
        return "DRAFT";
    }
    try {
        return cambc::to_string(cambc::self().champion_name());
    } catch (...) {
        return "UNKNOWN";
    }
}

inline void log(const char* msg) {
    if constexpr (kEnableLogging) {
        std::cout << "[" << get_champion_name() << "] " << msg << std::endl;
    }
}

inline void log_fuel(const char* msg) {
    if constexpr (kEnableLogging && kLogFuel) {
        std::cout << "[" << get_champion_name() << "][FUEL] " << msg << std::endl;
    }
}

inline void log_pathfinding(const char* msg) {
    if constexpr (kEnableLogging && kLogPathfinding) {
        std::cout << "[" << get_champion_name() << "][PATH] " << msg << std::endl;
    }
}

inline void log_combat(const char* msg) {
    if constexpr (kEnableLogging && kLogCombat) {
        std::cout << "[" << get_champion_name() << "][COMBAT] " << msg << std::endl;
    }
}

inline void log_farm(const char* msg) {
    if constexpr (kEnableLogging && kLogFarm) {
        std::cout << "[" << get_champion_name() << "][FARM] " << msg << std::endl;
    }
}

inline void log_draft(const char* msg) {
    if constexpr (kEnableLogging && kLogDraft) {
        std::cout << "[" << get_champion_name() << "][DRAFT] " << msg << std::endl;
    }
}

inline void log_init(const char* msg) {
    if constexpr (kEnableLogging && kLogInit) {
        std::cout << "[" << get_champion_name() << "][INIT] " << msg << std::endl;
    }
}

// Log with custom champion name (for use after storing name in BotMemory)
inline void log_with_name(const std::string& name, const std::string& msg) {
    if constexpr (kEnableLogging) {
        std::cout << "[" << name << "] " << msg << std::endl;
    }
}
}
