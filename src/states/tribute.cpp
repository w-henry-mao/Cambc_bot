#include "tribute.h"
#include "../nav/pathfinding.h"
#include "../core/rng.h"
#include <cambc.h>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace {
constexpr std::uint32_t kTributeGemThreshold = 50;
constexpr std::uint32_t kStaleGemRounds = 100;
constexpr std::uint32_t kChampionRespawnRounds = 60;
constexpr std::uint32_t kRespawnWaitWindow = 5;
constexpr std::uint32_t kLateFinalTributeRound = 1800;
constexpr std::uint32_t kLateFinalTributeGemThreshold = 100;
constexpr std::size_t kMaxTeamSize = 5;

enum class FinalTributeMode : std::uint8_t { inactive, recalling, escorting };
enum class FinalTributeStatus : std::uint8_t {
    unknown, alive_away, alive_at_spawn, dead_respawning_soon, dead_respawning_later
};

constexpr std::size_t kModeOffset = 0;
constexpr std::size_t kAltarXOffset = 1;
constexpr std::size_t kAltarYOffset = 2;
constexpr std::size_t kStatusLowOffset = 3;
constexpr std::size_t kStatusHighOffset = 4;
constexpr std::uint8_t kNoAltar = 0xff;
// Shared bytes 0-2 hold mode and altar coordinates; bytes 3-4 pack five 3-bit champion statuses.

FinalTributeMode final_tribute_mode() {
    return static_cast<FinalTributeMode>(static_cast<std::uint8_t>(cambc::shared_memory[kModeOffset]));
}

void set_final_tribute_mode(FinalTributeMode mode) {
    cambc::shared_memory[kModeOffset] = static_cast<std::uint8_t>(mode);
}

std::uint16_t read_statuses() {
    const auto low = static_cast<std::uint8_t>(cambc::shared_memory[kStatusLowOffset]);
    const auto high = static_cast<std::uint8_t>(cambc::shared_memory[kStatusHighOffset]);
    return static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8));
}

void write_statuses(std::uint16_t statuses) {
    cambc::shared_memory[kStatusLowOffset] = static_cast<std::uint8_t>(statuses & 0xff);
    cambc::shared_memory[kStatusHighOffset] = static_cast<std::uint8_t>(statuses >> 8);
}

FinalTributeStatus read_status(std::uint8_t slot) {
    const auto statuses = read_statuses();
    return static_cast<FinalTributeStatus>((statuses >> (slot * 3)) & 0x7);
}

void write_status(std::uint8_t slot, FinalTributeStatus status) {
    auto statuses = read_statuses();
    const auto shift = slot * 3;
    const auto mask = static_cast<std::uint16_t>(0x7u << shift);
    statuses = static_cast<std::uint16_t>((statuses & ~mask) |
        (static_cast<std::uint16_t>(status) << shift));
    write_statuses(statuses);
}

void clear_final_tribute_state() {
    cambc::shared_memory[kAltarXOffset] = kNoAltar;
    cambc::shared_memory[kAltarYOffset] = kNoAltar;
    write_statuses(0);
}

void activate_final_tribute() {
    clear_final_tribute_state();
    set_final_tribute_mode(FinalTributeMode::recalling);
}

std::vector<cambc::champion> allied_champions() {
    auto champions = cambc::champions(cambc::team::ally);
    std::sort(champions.begin(), champions.end(), [](const auto& a, const auto& b) {
        return static_cast<int>(a.champion_name()) < static_cast<int>(b.champion_name());
    });
    return champions;
}

void initialize_roster(BotMemory& memory) {
    const auto champions = allied_champions();
    if (champions.empty() || champions.size() > kMaxTeamSize) return;
    const auto type = cambc::self().champion_name();
    for (std::size_t slot = 0; slot < champions.size(); ++slot) {
        if (champions[slot].champion_name() != type) continue;
        memory.final_tribute_slot = static_cast<std::uint8_t>(slot);
        memory.final_tribute_roster_mask = static_cast<std::uint8_t>((1u << champions.size()) - 1u);
        memory.final_tribute_roster_initialized = true;
        return;
    }
}

std::optional<cambc::vec2> closest_altar(const cambc::vec2& position) {
    const auto& altars = cambc::map::altars();
    if (altars.is_empty()) return std::nullopt;

    cambc::bitboard blocked = cambc::map::walls() | cambc::map::holes() |
        altars | cambc::map::spawn_region(cambc::team::enemy);
    cambc::bitboard champion_tiles;
    cambc::bitboard camp_tiles;
    cambc::bitboard teleport_tiles;
    cambc::vision::champion_tiles_into(champion_tiles);
    cambc::vision::camp_tiles_into(camp_tiles);
    cambc::vision::teleport_tiles_into(teleport_tiles);
    blocked |= champion_tiles;
    blocked |= camp_tiles;
    blocked |= teleport_tiles;
    blocked.set(position, false);

    cambc::bitboard roots;
    roots.set(position);
    cambc::bitboard visited;
    cambc::distance_field field;
    field.compute(blocked, roots, visited);

    std::optional<cambc::vec2> best_altar;
    std::uint16_t best_distance = cambc::distance_field::unreachable;
    for (const auto altar : altars.positions()) {
        cambc::bitboard approach_tiles;
        approach_tiles.draw_chebyshev(altar, 1.0f);
        approach_tiles &= ~altars;
        approach_tiles &= ~blocked;
        for (const auto tile : approach_tiles.positions()) {
            const auto distance = field.get(tile);
            if (distance != cambc::distance_field::unreachable && distance < best_distance) {
                best_distance = distance;
                best_altar = altar;
            }
        }
    }
    return best_altar;
}

bool is_at_ally_spawn(const cambc::vec2& position) {
    return cambc::map::spawn_region(cambc::team::ally).is_set(position);
}

void publish_final_tribute_status(BotMemory& memory) {
    if (final_tribute_mode() == FinalTributeMode::inactive ||
        memory.final_tribute_slot >= kMaxTeamSize) return;

    auto& me = cambc::self();
    if (me.is_alive()) {
        memory.final_tribute_death_round_initialized = false;
        const auto status = is_at_ally_spawn(me.position())
            ? FinalTributeStatus::alive_at_spawn : FinalTributeStatus::alive_away;
        write_status(memory.final_tribute_slot, status);
        if (me.champion_name() == cambc::champion_type::prism &&
            final_tribute_mode() == FinalTributeMode::recalling &&
            status == FinalTributeStatus::alive_at_spawn &&
            !me.is_recalling() &&
            cambc::shared_memory[kAltarXOffset] == kNoAltar) {
            const auto altar = closest_altar(me.position());
            if (altar) {
                cambc::shared_memory[kAltarXOffset] = static_cast<std::uint8_t>(altar->x);
                cambc::shared_memory[kAltarYOffset] = static_cast<std::uint8_t>(altar->y);
            }
        }
        return;
    }

    const auto round = cambc::round_number();
    if (!memory.final_tribute_death_round_initialized) {
        memory.final_tribute_death_round = round;
        memory.final_tribute_death_round_initialized = true;
    }
    const auto dead_rounds = round - memory.final_tribute_death_round;
    const auto status = dead_rounds + kRespawnWaitWindow >= kChampionRespawnRounds
        ? FinalTributeStatus::dead_respawning_soon
        : FinalTributeStatus::dead_respawning_later;
    write_status(memory.final_tribute_slot, status);
}

bool all_required_champions_ready(const BotMemory& memory) {
    if (memory.final_tribute_roster_mask == 0) return false;
    const auto champions = allied_champions();
    if (champions.size() > kMaxTeamSize) return false;
    for (std::size_t slot = 0; slot < champions.size(); ++slot) {
        const auto status = read_status(static_cast<std::uint8_t>(slot));
        const auto& champion = champions[slot];
        if (champion.is_alive()) {
            if (status != FinalTributeStatus::alive_at_spawn) return false;
        } else if (status != FinalTributeStatus::dead_respawning_later) {
            return false;
        }
    }
    return true;
}

cambc::bitboard escort_blockers(const cambc::vec2& position) {
    cambc::bitboard blocked = cambc::map::walls() | cambc::map::holes() |
        cambc::map::altars() | cambc::map::spawn_region(cambc::team::enemy);
    cambc::bitboard champion_tiles;
    cambc::bitboard camp_tiles;
    cambc::bitboard teleport_tiles;
    cambc::vision::champion_tiles_into(champion_tiles);
    cambc::vision::camp_tiles_into(camp_tiles);
    cambc::vision::teleport_tiles_into(teleport_tiles);
    blocked |= champion_tiles;
    blocked |= camp_tiles;
    blocked |= teleport_tiles;
    blocked.set(position, false);
    return blocked;
}

bool guard_altar(BotMemory& memory, cambc::vec2 altar) {
    auto& me = cambc::self();
    cambc::bitboard guard_tiles;
    guard_tiles.draw_chebyshev(altar, 2.0f);
    cambc::bitboard blocked = escort_blockers(me.position());
    guard_tiles &= ~blocked;
    if (guard_tiles.is_empty()) return true;

    const auto position = me.position();
    if (guard_tiles.is_set(position)) {
        if (!me.can_move()) return true;
        std::vector<cambc::vec2> moves;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                const cambc::vec2 next{position.x + dx, position.y + dy};
                if (cambc::map::is_in_bounds(next) && guard_tiles.is_set(next)) moves.push_back(next);
            }
        }
        if (!moves.empty()) {
            const auto next = moves[random_number(static_cast<unsigned>(moves.size()))];
            me.move({next.x - position.x, next.y - position.y});
        }
        return true;
    }

    cambc::bitboard roots;
    roots.set(position);
    cambc::bitboard visited;
    cambc::distance_field field;
    field.compute(blocked, roots, visited);
    std::optional<cambc::vec2> destination;
    std::uint16_t nearest = cambc::distance_field::unreachable;
    for (const auto tile : guard_tiles.positions()) {
        const auto distance = field.get(tile);
        if (distance < nearest) {
            nearest = distance;
            destination = tile;
        }
    }
    if (destination) nav::move_towards(memory.movement, *destination, true, false);
    return true;
}

}

void update_tribute_progress(BotMemory& memory) {
    if (!memory.tribute_eligible) {

        return;
    }
    const auto gems = cambc::self().gem_count();
    const auto round = cambc::round_number();

    if (!memory.gem_progress_initialized || gems > memory.last_carried_gems) {
        memory.last_gem_increase_round = round;
    }
    memory.gem_progress_initialized = true;
    memory.last_carried_gems = gems;
    memory.carried_gems = gems;
    if (gems == 0) {
        memory.tribute_altar.reset();
        memory.last_gem_increase_round = round;
    }

}

void update_final_tribute(BotMemory& memory) {
    auto& me = cambc::self();
    const bool alive = me.is_alive();
    if (alive) {
        memory.final_tribute_death_round_initialized = false;
    } else if (!memory.final_tribute_death_round_initialized) {
        memory.final_tribute_death_round = cambc::round_number();
        memory.final_tribute_death_round_initialized = true;
    }

    if (me.champion_name() == cambc::champion_type::prism) {
        if (!alive) {
            set_final_tribute_mode(FinalTributeMode::inactive);
            memory.final_tribute_resume_round = 0;
            return;
        }

        const auto combined_gems = me.gem_count() + cambc::global_submitted_gems(cambc::team::ally);
        const auto round = cambc::round_number();
        const bool final_tribute_threshold_met = combined_gems >= 1000 ||
            (round >= kLateFinalTributeRound && me.gem_count() >= kLateFinalTributeGemThreshold);
        if (memory.final_tribute_resume_round != 0) {
            if (round < memory.final_tribute_resume_round) return;
            memory.final_tribute_resume_round = 0;
            if (final_tribute_threshold_met) {
                if (!memory.final_tribute_roster_initialized) initialize_roster(memory);
                if (memory.final_tribute_roster_initialized) activate_final_tribute();
            }
            else set_final_tribute_mode(FinalTributeMode::inactive);
        } else if (final_tribute_mode() == FinalTributeMode::inactive && final_tribute_threshold_met) {
            if (!memory.final_tribute_roster_initialized) initialize_roster(memory);
            if (memory.final_tribute_roster_initialized) activate_final_tribute();
        }
    }

    if (final_tribute_mode() == FinalTributeMode::inactive) return;
    if (!memory.final_tribute_roster_initialized) initialize_roster(memory);
    if (!memory.final_tribute_roster_initialized) return;
    publish_final_tribute_status(memory);
}

bool final_tribute_active() {
    return final_tribute_mode() != FinalTributeMode::inactive;
}

bool final_tribute_recall_phase() {
    return final_tribute_mode() == FinalTributeMode::recalling;
}

void pause_final_tribute_for_recall(BotMemory& memory) {
    if (cambc::self().champion_name() != cambc::champion_type::prism || !final_tribute_active()) return;
    memory.final_tribute_resume_round = cambc::round_number() + 1;
    set_final_tribute_mode(FinalTributeMode::inactive);
}

bool final_tribute_turn(BotMemory& memory) {
    auto mode = final_tribute_mode();
    if (mode == FinalTributeMode::inactive) return false;
    if (memory.final_tribute_slot >= kMaxTeamSize) return true;

    if (mode == FinalTributeMode::recalling) {
        if (cambc::self().champion_name() != cambc::champion_type::prism ||
            !all_required_champions_ready(memory) ||
            cambc::shared_memory[kAltarXOffset] == kNoAltar ||
            cambc::shared_memory[kAltarYOffset] == kNoAltar) return true;
        mode = FinalTributeMode::escorting;
        set_final_tribute_mode(mode);
    }

    const cambc::vec2 altar{
        static_cast<std::int32_t>(static_cast<std::uint8_t>(cambc::shared_memory[kAltarXOffset])),
        static_cast<std::int32_t>(static_cast<std::uint8_t>(cambc::shared_memory[kAltarYOffset]))
    };
    auto& me = cambc::self();
    if (me.champion_name() == cambc::champion_type::prism) {
        if (me.is_submitting_gems()) return true;
        if (me.gem_count() > 0 && me.position().chebyshev_distance(altar) <= 1) {
            try {
                me.submit_gems(altar);
            } catch (const cambc::error&) { }
        } else if (me.position().chebyshev_distance(altar) > 1) {
            nav::move_towards(memory.movement, altar, true, false);
        }
        return true;
    }

    if (me.is_submitting_gems()) {
        try {
            me.cancel_submit_gems();
        } catch (const cambc::error&) { }
    }
    return guard_altar(memory, altar);
}

bool tribute_turn(BotMemory& memory) {
    if (!memory.tribute_eligible) {

        return false;
    }
    auto& me = cambc::self();

    // An existing channel keeps running unless recall or combat took priority.
    if (me.is_submitting_gems()) {

        return true;
    }
    if (memory.carried_gems == 0) {

        return false;
    }

    const auto round = cambc::round_number();
    const bool enough_gems = memory.carried_gems >= kTributeGemThreshold;
    const bool gems_stale = memory.gem_progress_initialized &&
        round - memory.last_gem_increase_round >= kStaleGemRounds;
    if (!enough_gems && !gems_stale) {

        return false;
    }

    if (!memory.tribute_altar) {
        memory.tribute_altar = closest_altar(me.position());
        if (!memory.tribute_altar) {

            return false;
        }
    }

    const auto altar = *memory.tribute_altar;
    const auto position = me.position();
    if (position.chebyshev_distance(altar) <= 1) {
        try {
            // The engine converts this attempt into an attack if an enemy lock
            // occupies the altar, so keep selecting this altar until it clears.
            me.submit_gems(altar);

        } catch (const cambc::error&) {

            // Keep the target and retry after transient action/vision failures.
        }
        return true;
    }

    nav::move_towards(memory.movement, altar, true, false);
    return true;
}
