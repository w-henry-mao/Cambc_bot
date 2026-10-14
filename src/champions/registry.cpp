#include "champion_profile.h"
#include "registry.h"
#include <cambc.h>
#include <unordered_map>
extern ChampionProfile ryth_profile; extern ChampionProfile stellar_profile;
extern ChampionProfile tavi_profile; extern ChampionProfile willow_profile;
extern ChampionProfile cassi_profile; extern ChampionProfile issac_profile;
extern ChampionProfile prism_profile; extern ChampionProfile vesper_vale_profile;
extern ChampionProfile vendetta_profile; extern ChampionProfile marcella_profile;
extern ChampionProfile ren_profile; extern ChampionProfile varg_profile;
static const std::unordered_map<cambc::champion_type, ChampionProfile*> champion_registry = {
    {cambc::champion_type::ryth, &ryth_profile}, {cambc::champion_type::stellar, &stellar_profile},
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
