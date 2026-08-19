#include <gtest/gtest.h>

extern "C" {
#include "../src/bloomRa/bloom_ra.h"
}

TEST(BloomRaTest, StatusIsVersionedAndOfflineWithoutConfiguration)
{
    BloomRaStatus status = {};
    bloom_ra_get_status(&status);
    EXPECT_EQ(1, status.schema);
    EXPECT_EQ(0, status.enabled);
    EXPECT_STREQ("not_configured", status.state);
    EXPECT_STREQ("not_implemented", status.catalog_status);
    EXPECT_EQ(0UL, status.indexed_games);
    EXPECT_EQ(0UL, status.identified_games);
}

TEST(BloomRaTest, ValidGameIdReturnsUnindexedWithoutInventingRaIdentity)
{
    const char *game_id =
        "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    BloomRaGame game = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_get_game(game_id, &game, error, sizeof(error))) << error;
    EXPECT_STREQ(game_id, game.game_id);
    EXPECT_STREQ("unindexed", game.status);
    EXPECT_EQ(0, game.has_ra_badge);
    EXPECT_EQ(0UL, game.achievement_count);
}

TEST(BloomRaTest, InvalidGameIdFailsClosed)
{
    BloomRaGame game = {};
    char error[128] = {};
    EXPECT_NE(0, bloom_ra_get_game("bloom-game-v1:not-a-hash", &game, error, sizeof(error)));
    EXPECT_STREQ("invalid Bloom GameID", error);
}

TEST(BloomRaTest, MissingOutputFailsClosed)
{
    const char *game_id =
        "bloom-game-v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char error[128] = {};
    EXPECT_NE(0, bloom_ra_get_game(game_id, nullptr, error, sizeof(error)));
    EXPECT_STREQ("missing game output", error);
}
