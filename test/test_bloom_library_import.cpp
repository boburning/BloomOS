#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sqlite3/sqlite3.h>
#include <unistd.h>

extern "C" {
#include "../src/bloomLibrary/bloom_library_database.h"
#include "../src/bloomLibrary/bloom_library_import.h"
}

class BloomLibraryImportTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root_ = std::filesystem::temp_directory_path() /
                ("bloom-library-import-" + std::to_string(getpid()) + "-" +
                 std::to_string(reinterpret_cast<uintptr_t>(this)));
        emu_ = root_ / "Emu";
        app_ = root_ / "App";
        catalog_ = root_ / "system-catalog.json";
        std::filesystem::create_directories(emu_);
        std::filesystem::create_directories(app_);
        write_catalog();
        write_system("GBA", "GBA", "Game Boy Advance");
        write_system("GB", "GB", "Game Boy");
        write_app("Tweaks", "Tweaks");
        ASSERT_EQ(SQLITE_OK,
                  bloom_library_database_open((root_ / "catalog.sqlite3").c_str(), &database_));
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

    void write_catalog(const std::string &entries =
                           R"({"folder":"GBA","system_id":"gba"},)"
                           R"({"folder":"GB","system_id":"gb"})")
    {
        write(catalog_, R"({"schema":1,"entries":[)" + entries + "]}");
    }

    void write_system(const std::string &folder, const std::string &rom_folder,
                      const std::string &label)
    {
        write(emu_ / folder / "config.json",
              R"({"label":")" + label + R"(","rompath":"../../Roms/)" + rom_folder +
                  R"(","imgpath":"../../Roms/)" + rom_folder +
                  R"(/Imgs","launch":"launch.sh","extlist":"zip|rom"})");
    }

    void write_app(const std::string &folder, const std::string &label)
    {
        write(app_ / folder / "config.json",
              R"({"label":")" + label +
                  R"(","icon":"../../Icons/Default/app/tweaks.png","launch":"launch.sh"})");
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

    int import(BloomLibraryImportResult *result, char *error, size_t error_size)
    {
        return bloom_library_import_onion(database_, catalog_.c_str(), emu_.c_str(), app_.c_str(),
                                          result, error, error_size);
    }

    std::filesystem::path root_;
    std::filesystem::path emu_;
    std::filesystem::path app_;
    std::filesystem::path catalog_;
    sqlite3 *database_ = nullptr;
};

TEST_F(BloomLibraryImportTest, ImportsNormalizedSystemsAndAppsIdempotently)
{
    BloomLibraryImportResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    EXPECT_EQ(1, result.changed);
    EXPECT_EQ(1, result.generation);
    EXPECT_EQ(2, result.systems);
    EXPECT_EQ(1, result.apps);
    EXPECT_EQ("GBA", text("SELECT rom_path FROM systems WHERE system_id='gba'"));
    EXPECT_EQ("Emu/GBA/launch.sh",
              text("SELECT launch_path FROM systems WHERE system_id='gba'"));
    EXPECT_EQ("bloom-app-v1:Tweaks", text("SELECT app_id FROM apps"));
    EXPECT_EQ("Icons/Default/app/tweaks.png", text("SELECT icon_path FROM apps"));
    EXPECT_EQ("mainui-dependent", text("SELECT compatibility FROM apps"));

    result = {};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    EXPECT_EQ(0, result.changed);
    EXPECT_EQ(1, result.generation);
}

TEST_F(BloomLibraryImportTest, ImportsOnlyExplicitCompatibilityClasses)
{
    write(app_ / "Tweaks" / "config.json",
          R"({"label":"Tweaks","launch":"launch.sh","bloom_compatibility":"onion-compatible"})");
    BloomLibraryImportResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    EXPECT_EQ("onion-compatible", text("SELECT compatibility FROM apps"));

    write(app_ / "Tweaks" / "config.json",
          R"({"label":"Tweaks","launch":"launch.sh","bloom_compatibility":"probably-safe"})");
    EXPECT_NE(SQLITE_OK, import(&result, error, sizeof(error)));
    EXPECT_EQ("onion-compatible", text("SELECT compatibility FROM apps"));
}

TEST_F(BloomLibraryImportTest, CatalogCanDeclareDistinctRomFolder)
{
    write_catalog(R"({"folder":"GBA","rom_folder":"ADVANCE","system_id":"gba"},)"
                  R"({"folder":"GB","system_id":"gb"})");
    write_system("GBA", "ADVANCE", "Game Boy Advance");
    BloomLibraryImportResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    EXPECT_EQ("ADVANCE", text("SELECT rom_path FROM systems WHERE system_id='gba'"));
}

TEST_F(BloomLibraryImportTest, ChangedAndRemovedEntriesAdvanceOneGeneration)
{
    BloomLibraryImportResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    write_system("GBA", "GBA", "Game Boy Advance Updated");
    std::filesystem::remove_all(emu_ / "GB");
    write_app("Tweaks", "Bloom Tweaks");

    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    EXPECT_EQ(1, result.changed);
    EXPECT_EQ(2, result.generation);
    EXPECT_EQ(1, result.systems);
    EXPECT_EQ(1, scalar("SELECT present FROM systems WHERE system_id='gba'"));
    EXPECT_EQ(0, scalar("SELECT present FROM systems WHERE system_id='gb'"));
    EXPECT_EQ("Bloom Tweaks", text("SELECT label FROM apps"));
}

TEST_F(BloomLibraryImportTest, InvalidInputRollsBackKnownGoodCatalog)
{
    BloomLibraryImportResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    write(app_ / "Tweaks" / "config.json", "{not-json");

    EXPECT_NE(SQLITE_OK, import(&result, error, sizeof(error)));
    EXPECT_EQ(1, scalar("SELECT generation FROM library_state WHERE id=1"));
    EXPECT_EQ(2, scalar("SELECT COUNT(*) FROM systems WHERE present=1"));
    EXPECT_EQ("Tweaks", text("SELECT label FROM apps WHERE present=1"));
}

TEST_F(BloomLibraryImportTest, TrailingTraversalComponentFailsClosed)
{
    write(emu_ / "GBA" / "config.json",
          R"({"label":"GBA","rompath":"../../Roms/GBA/..","imgpath":"../../Roms/GBA/Imgs","launch":"launch.sh","extlist":"gba"})");
    BloomLibraryImportResult result{};
    char error[160]{};
    EXPECT_EQ(SQLITE_CORRUPT, import(&result, error, sizeof(error)));
    EXPECT_EQ(0, scalar("SELECT generation FROM library_state WHERE id=1"));
    EXPECT_EQ(0, scalar("SELECT COUNT(*) FROM systems"));
}

TEST_F(BloomLibraryImportTest, EmptyDiscoveryCannotReplaceKnownGoodCatalog)
{
    BloomLibraryImportResult result{};
    char error[160]{};
    ASSERT_EQ(SQLITE_OK, import(&result, error, sizeof(error))) << error;
    std::filesystem::remove_all(emu_ / "GBA");
    std::filesystem::remove_all(emu_ / "GB");

    EXPECT_NE(SQLITE_OK, import(&result, error, sizeof(error)));
    EXPECT_EQ(1, scalar("SELECT generation FROM library_state WHERE id=1"));
    EXPECT_EQ(2, scalar("SELECT COUNT(*) FROM systems WHERE present=1"));
}

TEST_F(BloomLibraryImportTest, DuplicateCatalogIdentityFailsClosed)
{
    write_catalog(R"({"folder":"GBA","system_id":"gba"},)"
                  R"({"folder":"GB","system_id":"gba"})");
    BloomLibraryImportResult result{};
    char error[160]{};
    EXPECT_EQ(SQLITE_CORRUPT, import(&result, error, sizeof(error)));
    EXPECT_EQ(0, scalar("SELECT generation FROM library_state WHERE id=1"));
}
