#include "test.h"
#include "combat/model.h"
TEST(test_model_defaults) {
    CHECK(FightOptions{}.round_cap == 60);
    CHECK(FightOptions{}.include_camps == false);
}
