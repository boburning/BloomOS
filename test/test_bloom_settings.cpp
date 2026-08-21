#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "../include/cjson/cJSON.h"
#include "../src/bloomSettings/bloom_settings.h"
}

class BloomSettingsTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() /
               ("bloom-settings-" + std::to_string(getpid()) + "-" +
                std::to_string(reinterpret_cast<uintptr_t>(this)));
        config = root / "config";
        std::filesystem::create_directories(config / "battery");
        std::filesystem::create_directories(config / "startup");
        settings = root / "settings.json";
        snapshot = root / "onion-system.snapshot.json";
        system = root / "system.json";
    }

    void TearDown() override { std::filesystem::remove_all(root); }

    static void write(const std::filesystem::path &path, const std::string &content)
    {
        std::ofstream stream(path, std::ios::binary);
        stream << content;
        stream.close();
        ASSERT_TRUE(stream.good());
    }

    cJSON *load_settings()
    {
        std::ifstream stream(settings, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());
        return cJSON_ParseWithLength(content.c_str(), content.size());
    }

    std::filesystem::path root;
    std::filesystem::path config;
    std::filesystem::path settings;
    std::filesystem::path snapshot;
    std::filesystem::path system;
};

TEST_F(BloomSettingsTest, ImportsKnownValuesAndRetainsUnknownLegacyData)
{
    const std::string legacy =
        R"({"vol":11,"mute":0,"brightness":4,"wifi":1,"language":"fr.lang","theme":"/Themes/Bloom/","fontsize":32,"unknown_future_key":{"kept":true}})";
    write(system, legacy);
    write(config / "keymap.json", R"({"mainui_single_press":2,"unknown_binding":"kept"})");
    write(config / ".showRecents", "");
    write(config / ".muteVolume", "");
    write(config / "battery/warnAt", "17\n");
    write(config / "startup/tab", "3");

    BloomSettingsImportResult result{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)))
        << error;
    EXPECT_EQ(1, result.imported);
    EXPECT_EQ(0, result.used_defaults);
    EXPECT_EQ(1, result.legacy_snapshot_written);

    int schema = 0;
    char source_name[32] = {};
    char authority[16] = {};
    ASSERT_EQ(0, bloom_settings_status(settings.c_str(), &schema, source_name,
                                       sizeof(source_name), authority, sizeof(authority), error,
                                       sizeof(error)));
    EXPECT_EQ(1, schema);
    EXPECT_STREQ("onion", source_name);
    EXPECT_STREQ("legacy", authority);

    cJSON *root_json = load_settings();
    ASSERT_TRUE(cJSON_IsObject(root_json));
    EXPECT_EQ(1, cJSON_GetObjectItem(root_json, "schema")->valueint);
    EXPECT_STREQ("legacy", cJSON_GetObjectItem(root_json, "authority")->valuestring);
    cJSON *device = cJSON_GetObjectItem(root_json, "device");
    EXPECT_EQ(11, cJSON_GetObjectItem(device, "volume")->valueint);
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(device, "mute")));
    EXPECT_EQ(4, cJSON_GetObjectItem(device, "brightness")->valueint);
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(device, "wifi_enabled")));
    cJSON *interface = cJSON_GetObjectItem(root_json, "interface");
    EXPECT_STREQ("fr.lang", cJSON_GetObjectItem(interface, "language")->valuestring);
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(interface, "show_recents")));
    cJSON *behavior = cJSON_GetObjectItem(root_json, "behavior");
    EXPECT_EQ(17, cJSON_GetObjectItem(behavior, "low_battery_warn_at")->valueint);
    EXPECT_EQ(3, cJSON_GetObjectItem(behavior, "startup_tab")->valueint);
    cJSON *compatibility = cJSON_GetObjectItem(root_json, "compatibility");
    cJSON *legacy_json = cJSON_GetObjectItem(compatibility, "onion_system");
    EXPECT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(legacy_json, "unknown_future_key")));
    cJSON *legacy_keymap = cJSON_GetObjectItem(compatibility, "onion_keymap");
    EXPECT_STREQ("kept", cJSON_GetObjectItem(legacy_keymap, "unknown_binding")->valuestring);
    cJSON_Delete(root_json);

    std::ifstream snapshot_stream(snapshot, std::ios::binary);
    std::string snapshot_content((std::istreambuf_iterator<char>(snapshot_stream)),
                                 std::istreambuf_iterator<char>());
    EXPECT_EQ(legacy, snapshot_content);
    auto permissions = std::filesystem::status(settings).permissions();
    EXPECT_EQ(std::filesystem::perms::none,
              permissions & (std::filesystem::perms::group_all |
                             std::filesystem::perms::others_all));
}

TEST_F(BloomSettingsTest, RepeatedImportDoesNotOverwriteCanonicalStateOrSnapshot)
{
    write(system, R"({"vol":5})");
    BloomSettingsImportResult first{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &first, error, sizeof(error)));
    std::ifstream first_stream(settings);
    std::string first_content((std::istreambuf_iterator<char>(first_stream)),
                              std::istreambuf_iterator<char>());
    write(system, R"({"vol":19})");
    BloomSettingsImportResult second{};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &second, error, sizeof(error)));
    EXPECT_EQ(0, second.imported);
    EXPECT_EQ(0, second.legacy_snapshot_written);
    std::ifstream second_stream(settings);
    std::string second_content((std::istreambuf_iterator<char>(second_stream)),
                               std::istreambuf_iterator<char>());
    EXPECT_EQ(first_content, second_content);
    std::ifstream snapshot_stream(snapshot);
    std::string snapshot_content((std::istreambuf_iterator<char>(snapshot_stream)),
                                 std::istreambuf_iterator<char>());
    EXPECT_EQ(R"({"vol":5})", snapshot_content);
}

TEST_F(BloomSettingsTest, MissingLegacyFilesProduceExplicitDefaults)
{
    BloomSettingsImportResult result{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)))
        << error;
    EXPECT_EQ(1, result.imported);
    EXPECT_EQ(1, result.used_defaults);
    EXPECT_FALSE(std::filesystem::exists(snapshot));
    cJSON *root_json = load_settings();
    cJSON *device = cJSON_GetObjectItem(root_json, "device");
    EXPECT_EQ(20, cJSON_GetObjectItem(device, "volume")->valueint);
    EXPECT_EQ(7, cJSON_GetObjectItem(device, "brightness")->valueint);
    cJSON_Delete(root_json);
}

TEST_F(BloomSettingsTest, InvalidOrNewerCanonicalSchemaFailsClosed)
{
    char source[32] = {};
    char authority[16] = {};
    char error[160] = {};
    int schema = 0;
    write(settings, R"({"schema":2,"source":{"kind":"future"}})");
    EXPECT_NE(0, bloom_settings_status(settings.c_str(), &schema, source, sizeof(source), authority,
                                       sizeof(authority), error, sizeof(error)));
    BloomSettingsImportResult result{};
    EXPECT_NE(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)));
    write(settings, "not-json");
    EXPECT_NE(0, bloom_settings_status(settings.c_str(), &schema, source, sizeof(source), authority,
                                       sizeof(authority), error, sizeof(error)));
    write(settings,
          R"({"schema":1,"generation":1,"authority":"legacy","source":{"kind":"bad\"source"},"device":{},"interface":{},"behavior":{},"compatibility":{}})");
    EXPECT_NE(0, bloom_settings_status(settings.c_str(), &schema, source, sizeof(source), authority,
                                       sizeof(authority), error, sizeof(error)));
    write(settings,
          R"({"schema":1,"generation":1,"authority":"surprise","source":{"kind":"onion"},"device":{},"interface":{},"behavior":{},"compatibility":{}})");
    EXPECT_NE(0, bloom_settings_status(settings.c_str(), &schema, source, sizeof(source), authority,
                                       sizeof(authority), error, sizeof(error)));
}

TEST_F(BloomSettingsTest, RejectsSymlinkedLegacyInput)
{
    auto outside = root / "outside.json";
    write(outside, R"({"vol":8})");
    std::filesystem::create_symlink(outside, system);
    BloomSettingsImportResult result{};
    char error[160] = {};
    EXPECT_NE(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(settings));
}

TEST_F(BloomSettingsTest, InvalidLegacyInputCannotPublishPartialSettings)
{
    write(system, "not-json");
    BloomSettingsImportResult result{};
    char error[160] = {};
    EXPECT_NE(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(settings));
    EXPECT_FALSE(std::filesystem::exists(snapshot));
}

TEST_F(BloomSettingsTest, RefusesUnsafePreexistingSnapshot)
{
    write(system, R"({"vol":8})");
    auto outside = root / "outside.json";
    write(outside, "do-not-overwrite");
    std::filesystem::create_symlink(outside, snapshot);
    BloomSettingsImportResult result{};
    char error[160] = {};
    EXPECT_NE(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(settings));
    std::ifstream stream(outside);
    std::string content((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ("do-not-overwrite", content);
}
