#pragma once
// Reading and writing the shared header word (bytes 0-3, layout in comms/shared_memory.h).
// Turns are sequential, so a read-modify-write of one field leaves the others intact.
#include <cstdint>

std::uint32_t read_team_header();
void write_team_header(std::uint32_t word);
// Once per turn for every champion (alive or dead): clear an expired farm target or fight call.
void sweep_team_header();
