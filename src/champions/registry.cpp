#include "champion_profile.h"
#include "registry.h"
#include <cambc.h>
#include <unordered_map>

static ChampionProfile heart_of_ruin_profile;
static ChampionProfile ryth_profile;
static ChampionProfile stellar_profile;
static ChampionProfile tavi_profile;
static ChampionProfile willow_profile;
static ChampionProfile cassi_profile;
static ChampionProfile issac_profile;
static ChampionProfile prism_profile;
static ChampionProfile vesper_vale_profile;
static ChampionProfile vendetta_profile;
static ChampionProfile marcella_profile;
static ChampionProfile ren_profile;
static ChampionProfile varg_profile;

static const std::unordered_map<cambc::champion_type, ChampionProfile*> champion_registry = {
    {cambc::champion_type::heart_of_ruin, &heart_of_ruin_profile}, {cambc::champion_type::ryth, &ryth_profile}, {cambc::champion_type::stellar, &stellar_profile},
    {cambc::champion_type::tavi, &tavi_profile}, {cambc::champion_type::willow, &willow_profile},
    {cambc::champion_type::cassi, &cassi_profile}, {cambc::champion_type::issac, &issac_profile},
    {cambc::champion_type::prism, &prism_profile}, {cambc::champion_type::vesper_vale, &vesper_vale_profile},
    {cambc::champion_type::vendetta, &vendetta_profile}, {cambc::champion_type::marcella, &marcella_profile},
    {cambc::champion_type::ren, &ren_profile}, {cambc::champion_type::varg, &varg_profile},
};

const ChampionProfile* get_champion_profile(cambc::champion_type type) {
    auto it = champion_registry.find(type);
    return it != champion_registry.end() ? it->second : nullptr;
}
