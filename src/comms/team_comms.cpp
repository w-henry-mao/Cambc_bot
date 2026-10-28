#include "team_comms.h"

std::uint32_t read_team_header() {
    return 0;
}

void write_team_header(std::uint32_t word) {
}

void sweep_team_header() {
}

void write_rally(int x, int y) {
}

team_comms::RallyField read_rally(int max_age) {
    return team_comms::RallyField{};
}

void write_group2_target(int group_plus1) {
}

int read_group2_target(int max_age) {
    return 0;
}
