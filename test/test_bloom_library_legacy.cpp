#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sqlite3/sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomLibrary/bloom_library_database.h"
#include "../src/bloomLibrary/bloom_library_legacy.h"
}

class BloomLibraryLegacyTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-library-legacy-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        rom_root_ = root_ / "Roms";
        std::filesystem::create_directories(rom_root_ / "GBA");
        favorites_ = rom_root_ / "favourite.json";
        recents_ = rom_root_ / "recentlist.json";
        ASSERT_EQ(SQLITE_OK,
                  bloom_library_database_open((root_ / "catalog.sqlite3").c_str(), &database_));
        execute("INSERT INTO systems VALUES('gba','GBA','GBA',NULL,'launch','gba',1,2,1);"
                "INSERT INTO games VALUES('bloom-game-v1:one','gba','GBA/One.gba','One','one',"
                "NULL,3,4,1);"
                "INSERT INTO games VALUES('bloom-game-v1:two','gba','GBA/Two.gba','Two','two',"
                "NULL,3,4,1);");
    }

    void TearDown() override
    {
        if (database_ != nullptr)
            sqlite3_close(database_);
        std::filesystem::remove_all(root_);
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

    BloomLibraryLegacyResult import()
    {
        BloomLibraryLegacyResult result{};
        char error[160] = {};
        EXPECT_EQ(SQLITE_OK,
                  bloom_library_import_legacy(database_, rom_root_.c_str(), favorites_.c_str(),
                                              recents_.c_str(), &result, error, sizeof(error)))
            << error;
        return result;
    }

    std::filesystem::path root_;
    std::filesystem::path rom_root_;
    std::filesystem::path favorites_;
    std::filesystem::path recents_;
    sqlite3 *database_ = nullptr;
};

TEST_F(BloomLibraryLegacyTest, ImportsKnownFormatsAndRecordsEveryOutcome)
{
    const std::string root = rom_root_.string();
    std::ofstream(favorites_)
        << "{\"type\":5,\"rompath\":\"" << root << "/GBA/One.gba\"}\n"
        << "{\"type\":0,\"rompath\":\"" << root << "/GBA/One.gba\"}\n"
        << "{\"type\":5,\"rompath\":\"" << root << "/GBA/Missing.gba\"}\n"
        << "{\"type\":3,\"rompath\":\"" << root << "/GBA/Two.gba\"}\n"
        << "{\"type\":5,\"type\":5,\"rompath\":\"" << root << "/GBA/Two.gba\"}\n"
        << "not-json\n";
    std::ofstream(recents_)
        << "{\"type\":17,\"rompath\":\"/mnt/SDCARD/Emu/GBA/launch.sh:" << root
        << "/GBA/Two.gba\"}\n"
        << "{\"type\":1,\"rompath\":\"" << root << "/../outside.gba\"}\n";

    BloomLibraryLegacyResult result = import();
    EXPECT_EQ(1, result.favorites);
    EXPECT_EQ(1, result.recents);
    EXPECT_EQ(2, result.matched);
    EXPECT_EQ(1, result.unmatched);
    EXPECT_EQ(1, result.duplicates);
    EXPECT_EQ(4, result.invalid);
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM favorites"));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM recents"));
    EXPECT_EQ(8, scalar("SELECT COUNT(*) FROM legacy_items"));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM legacy_items WHERE status='unmatched' AND "
                        "legacy_identity='GBA/Missing.gba'"));
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM legacy_items WHERE legacy_identity LIKE '%outside%'"));
}

TEST_F(BloomLibraryLegacyTest, ReimportIsIdempotentAndMissingListsMeanEmptyState)
{
    std::ofstream(favorites_) << "{\"type\":5,\"rompath\":\"" << rom_root_.string()
                              << "/GBA/One.gba\"}\n";
    std::ofstream(recents_) << "{\"type\":5,\"rompath\":\"" << rom_root_.string()
                            << "/GBA/Two.gba\"}\n";
    BloomLibraryLegacyResult first = import();
    BloomLibraryLegacyResult second = import();
    EXPECT_EQ(first.favorites, second.favorites);
    EXPECT_EQ(first.recents, second.recents);
    EXPECT_EQ(2, scalar("SELECT COUNT(*) FROM legacy_items"));

    std::filesystem::remove(favorites_);
    std::filesystem::remove(recents_);
    BloomLibraryLegacyResult empty = import();
    EXPECT_EQ(0, empty.favorites);
    EXPECT_EQ(0, empty.recents);
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM favorites"));
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM recents"));
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM legacy_items"));
}

TEST_F(BloomLibraryLegacyTest, UnsafeInputPreservesPriorKnownGoodState)
{
    std::ofstream(favorites_) << "{\"type\":5,\"rompath\":\"" << rom_root_.string()
                              << "/GBA/One.gba\"}\n";
    std::ofstream recents_file(recents_);
    recents_file.close();
    ASSERT_EQ(1, import().favorites);
    std::filesystem::remove(favorites_);
    std::filesystem::create_symlink(root_ / "catalog.sqlite3", favorites_);

    BloomLibraryLegacyResult result{};
    char error[160] = {};
    EXPECT_EQ(SQLITE_CANTOPEN,
              bloom_library_import_legacy(database_, rom_root_.c_str(), favorites_.c_str(),
                                          recents_.c_str(), &result, error, sizeof(error)));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM favorites"));
    EXPECT_EQ(1, scalar("SELECT COUNT(*) FROM legacy_items WHERE kind='favorite'"));
}
