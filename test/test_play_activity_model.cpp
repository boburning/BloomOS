#include <gtest/gtest.h>

#include <sqlite3/sqlite3.h>

#include <cstring>

extern "C" {
#include "../src/playActivity/playActivityModel.h"
}

namespace {

TEST(PlayActivityModelTest, ReadsAllSelectedColumnsWithoutMutatingRomPath)
{
    sqlite3 *database = nullptr;
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database));
    ASSERT_EQ(SQLITE_OK,
              sqlite3_prepare_v2(database,
                                 "SELECT 7, 'GB', 'Pokemon Red', 'GB/Pokemon Red (USA).zip', 3, 900, 300, "
                                 "'2026-01-01 12:00:00', '2026-02-02 13:00:00'",
                                 -1, &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));

    PlayActivity *activity = play_activity_read_row(statement);

    ASSERT_NE(nullptr, activity);
    ASSERT_NE(nullptr, activity->rom);
    EXPECT_EQ(7, activity->rom->id);
    EXPECT_STREQ("GB/Pokemon Red (USA).zip", activity->rom->file_path);
    EXPECT_STREQ("/mnt/SDCARD/Roms/GB/Imgs/Pokemon Red (USA).png", activity->rom->image_path);
    EXPECT_STREQ("2026-01-01 12:00:00", activity->first_played_at);
    EXPECT_STREQ("2026-02-02 13:00:00", activity->last_played_at);
    EXPECT_EQ(3, activity->play_count);
    EXPECT_EQ(900, activity->play_time_total);
    EXPECT_EQ(300, activity->play_time_average);

    play_activity_free(activity);
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

TEST(PlayActivityModelTest, Pico8ImageIsTheRomAndInputIsImmutable)
{
    char rom[] = "PICO/Test Cart.p8";
    char output[4096] = {};
    ASSERT_EQ(0, play_activity_image_path(rom, output, sizeof(output)));
    EXPECT_STREQ("PICO/Test Cart.p8", rom);
    EXPECT_STREQ("/mnt/SDCARD/Roms/PICO/Test Cart.p8", output);
}

TEST(PlayActivityModelTest, DotsInALegitimateFilenameArePreserved)
{
    char output[4096] = {};
    ASSERT_EQ(0, play_activity_image_path("GB/Wait... What.zip", output, sizeof(output)));
    EXPECT_STREQ("/mnt/SDCARD/Roms/GB/Imgs/Wait... What.png", output);
    EXPECT_NE(0, play_activity_image_path("GB/../GBC/Test.zip", output, sizeof(output)));
}

TEST(PlayActivityModelTest, NullableDatabaseFieldsRemainNullAndAreFreeable)
{
    sqlite3 *database = nullptr;
    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &database));
    ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(database, "SELECT 1, NULL, NULL, NULL, 0, 0, 0, NULL, NULL", -1,
                                            &statement, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(statement));
    PlayActivity *activity = play_activity_read_row(statement);
    ASSERT_NE(nullptr, activity);
    EXPECT_EQ(nullptr, activity->rom->type);
    EXPECT_EQ(nullptr, activity->rom->name);
    EXPECT_EQ(nullptr, activity->rom->file_path);
    EXPECT_EQ(nullptr, activity->rom->image_path);
    EXPECT_EQ(nullptr, activity->first_played_at);
    EXPECT_EQ(nullptr, activity->last_played_at);
    play_activity_free(activity);
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

} // namespace
