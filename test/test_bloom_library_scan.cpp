#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sqlite3/sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomGameId/bloom_game_id.h"
#include "../src/bloomLibrary/bloom_library_database.h"
#include "../src/bloomLibrary/bloom_library_scan.h"
}

class BloomLibraryScanTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-library-scan-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        roms_ = root_ / "Roms";
        std::filesystem::create_directories(roms_ / "GBA" / "Imgs");
        std::filesystem::create_directories(roms_ / "GB");
        ASSERT_EQ(SQLITE_OK,
                  bloom_library_database_open((root_ / "catalog.sqlite3").c_str(), &database_));
        execute("INSERT INTO systems VALUES"
                "('gba','GBA','GBA','GBA/Imgs','Emu/GBA/launch.sh','gba|zip',1,2,1),"
                "('gb','GB','GB',NULL,'Emu/GB/launch.sh','gb|zip',1,2,1);"
                "UPDATE library_state SET status='ready' WHERE id=1;");
    }

    void TearDown() override
    {
        if (database_ != nullptr)
            sqlite3_close(database_);
        std::filesystem::remove_all(root_);
    }

    static void write(const std::filesystem::path &path, const std::string &content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(stream.is_open());
        stream << content;
        ASSERT_TRUE(stream.good());
    }

    void execute(const char *sql)
    {
        ASSERT_EQ(SQLITE_OK, sqlite3_exec(database_, sql, nullptr, nullptr, nullptr));
    }

    int scalar(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr));
        EXPECT_EQ(SQLITE_ROW, sqlite3_step(statement));
        int value = sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }

    std::string text(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr));
        EXPECT_EQ(SQLITE_ROW, sqlite3_step(statement));
        const unsigned char *value = sqlite3_column_text(statement, 0);
        std::string result = value == nullptr ? "" : reinterpret_cast<const char *>(value);
        sqlite3_finalize(statement);
        return result;
    }

    int scan(const char *system, BloomLibraryScanResult *result, char *error, size_t error_size)
    {
        return bloom_library_scan_games(database_, roms_.c_str(), system, result, error,
                                        error_size);
    }

    std::filesystem::path root_;
    std::filesystem::path roms_;
    sqlite3 *database_ = nullptr;
};

TEST_F(BloomLibraryScanTest, EnumeratesCanonicalGamesAndImagesIdempotently)
{
    write(roms_ / "GBA" / "Alpha.gba", "alpha");
    write(roms_ / "GBA" / "Archive.ZIP", "archive");
    write(roms_ / "GBA" / "notes.txt", "ignored");
    write(roms_ / "GBA" / ".hidden.gba", "ignored");
    write(roms_ / "GBA" / "Hacks" / "Beta.gba", "beta");
    write(roms_ / "GBA" / "Imgs" / "Alpha.png", "image");
    write(roms_ / "GB" / "Mono.gb", "mono");

    BloomLibraryScanResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, scan(nullptr, &result, error, sizeof(error))) << error;
    EXPECT_EQ(1, result.changed);
    EXPECT_EQ(1, result.generation);
    EXPECT_EQ(2, result.systems);
    EXPECT_EQ(4, result.games);
    EXPECT_EQ(0, result.errors);
    EXPECT_EQ("alpha", text("SELECT sort_title FROM games WHERE display_title='Alpha'"));
    EXPECT_EQ("GBA/Imgs/Alpha.png",
              text("SELECT image_path FROM games WHERE display_title='Alpha'"));
    EXPECT_EQ("GBA/Hacks/Beta.gba",
              text("SELECT normalized_rom_path FROM games WHERE display_title='Beta'"));

    char expected[BLOOM_GAME_ID_LENGTH + 1];
    char normalized[512];
    ASSERT_EQ(0, bloom_game_id_create("gba", "/mnt/SDCARD/Roms/GBA/Alpha.gba", expected,
                                      sizeof(expected), normalized, sizeof(normalized), error,
                                      sizeof(error)));
    EXPECT_EQ(expected, text("SELECT bloom_game_id FROM games WHERE display_title='Alpha'"));

    result = {};
    ASSERT_EQ(SQLITE_OK, scan(nullptr, &result, error, sizeof(error))) << error;
    EXPECT_EQ(0, result.changed);
    EXPECT_EQ(1, result.generation);
    EXPECT_EQ(4, scalar("SELECT COUNT(*) FROM games WHERE present=1"));
}

TEST_F(BloomLibraryScanTest, ScopedScanInvalidatesOnlyItsSystem)
{
    write(roms_ / "GBA" / "Old.gba", "old");
    write(roms_ / "GB" / "Keep.gb", "keep");
    BloomLibraryScanResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, scan(nullptr, &result, error, sizeof(error))) << error;
    std::filesystem::remove(roms_ / "GBA" / "Old.gba");
    write(roms_ / "GBA" / "New.gba", "new content");

    ASSERT_EQ(SQLITE_OK, scan("gba", &result, error, sizeof(error))) << error;
    EXPECT_EQ(1, result.changed);
    EXPECT_EQ(2, result.generation);
    EXPECT_EQ(1, result.systems);
    EXPECT_EQ(1, result.games);
    EXPECT_EQ(0, scalar("SELECT present FROM games WHERE display_title='Old'"));
    EXPECT_EQ(1, scalar("SELECT present FROM games WHERE display_title='New'"));
    EXPECT_EQ(1, scalar("SELECT present FROM games WHERE display_title='Keep'"));
}

TEST_F(BloomLibraryScanTest, EmptySystemMarksPriorRowsAbsent)
{
    write(roms_ / "GBA" / "Gone.gba", "gone");
    BloomLibraryScanResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, scan("gba", &result, error, sizeof(error))) << error;
    std::filesystem::remove(roms_ / "GBA" / "Gone.gba");

    ASSERT_EQ(SQLITE_OK, scan("gba", &result, error, sizeof(error))) << error;
    EXPECT_EQ(2, result.generation);
    EXPECT_EQ(0, result.games);
    EXPECT_EQ(0, scalar("SELECT present FROM games WHERE display_title='Gone'"));
}

TEST_F(BloomLibraryScanTest, SymlinkedContentIsNeverFollowed)
{
    write(root_ / "outside.gba", "private");
    std::filesystem::create_symlink(root_ / "outside.gba", roms_ / "GBA" / "linked.gba");
    BloomLibraryScanResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, scan("gba", &result, error, sizeof(error))) << error;
    EXPECT_EQ(1, result.errors);
    EXPECT_EQ(0, result.games);
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM games"));
}

TEST_F(BloomLibraryScanTest, UnsafeCatalogPathRollsBack)
{
    write(roms_ / "GB" / "Keep.gb", "keep");
    execute("UPDATE systems SET rom_path='../outside' WHERE system_id='gba'");
    BloomLibraryScanResult result{};
    char error[160]{};
    EXPECT_EQ(SQLITE_CORRUPT, scan(nullptr, &result, error, sizeof(error)));
    EXPECT_EQ(0, scalar("SELECT generation FROM library_state WHERE id=1"));
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM games"));
}

TEST_F(BloomLibraryScanTest, UnknownSystemAndSymlinkedRootFailClosed)
{
    BloomLibraryScanResult result{};
    char error[160]{};
    EXPECT_EQ(SQLITE_NOTFOUND, scan("missing", &result, error, sizeof(error)));
    auto linked = root_ / "linked-roms";
    std::filesystem::create_directory_symlink(roms_, linked);
    EXPECT_EQ(SQLITE_CANTOPEN,
              bloom_library_scan_games(database_, linked.c_str(), nullptr, &result, error,
                                       sizeof(error)));
}

TEST_F(BloomLibraryScanTest, HandlesTenThousandGameFixtureIncrementally)
{
    for (int index = 0; index < 10000; ++index) {
        char name[32];
        snprintf(name, sizeof(name), "game-%05d.gba", index);
        write(roms_ / "GBA" / name, "x");
    }
    std::filesystem::permissions(roms_ / "GBA" / "game-00000.gba",
                                 std::filesystem::perms::none);
    BloomLibraryScanResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, scan("gba", &result, error, sizeof(error))) << error;
    EXPECT_EQ(10000, result.games);
    EXPECT_EQ(1, result.generation);
    EXPECT_EQ(10000, scalar("SELECT COUNT(*) FROM games WHERE present=1"));

    ASSERT_EQ(SQLITE_OK, scan("gba", &result, error, sizeof(error))) << error;
    EXPECT_EQ(0, result.changed);
    EXPECT_EQ(1, result.generation);
    std::filesystem::permissions(roms_ / "GBA" / "game-00000.gba",
                                 std::filesystem::perms::owner_all);
}
