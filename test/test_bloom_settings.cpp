#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <sys/wait.h>
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
        std::filesystem::create_directories(config / "display");
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
        R"({"vol":11,"mute":0,"bgmvol":9,"brightness":4,"wifi":1,"hibernate":8,"lumination":6,"hue":9,"saturation":11,"contrast":12,"audiofix":0,"keymap":"CUSTOM","language":"fr.lang","theme":"/Themes/Bloom/","fontsize":32,"unknown_future_key":{"kept":true}})";
    write(system, legacy);
    write(config / "keymap.json",
          R"({"mainui_single_press":2,"mainui_long_press":4,"mainui_double_press":5,"ingame_single_press":6,"ingame_long_press":7,"ingame_double_press":8,"mainui_button_x":"Search","mainui_button_y":"Activity","unknown_binding":"kept"})");
    write(config / ".showRecents", "");
    write(config / ".showExpert", "");
    write(config / ".muteVolume", "");
    write(config / ".bgmMute", "");
    write(config / ".blfOn", "");
    write(config / ".blf", "");
    write(config / ".recIndicator", "");
    write(config / ".recHotkey", "");
    write(config / "battery/warnAt", "17\n");
    write(config / "battery/exitAt", "6");
    write(config / "startup/tab", "3");
    write(config / "startup/app", "2");
    write(config / "startup/addHours", "7");
    write(config / "vibration", "3");
    write(config / "pwmfrequency", "5");
    write(config / "display/blueLightLevel", "4");
    write(config / "display/blueLightRGB", "1122867");
    write(config / "display/blueLightTime", "21:30\n");
    write(config / "display/blueLightTimeOff", "07:15\n");
    write(config / "recCountdown", "4");

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
    EXPECT_EQ(9, cJSON_GetObjectItem(device, "background_music_volume")->valueint);
    EXPECT_EQ(4, cJSON_GetObjectItem(device, "brightness")->valueint);
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(device, "wifi_enabled")));
    EXPECT_EQ(8, cJSON_GetObjectItem(device, "sleep_minutes")->valueint);
    EXPECT_EQ(6, cJSON_GetObjectItem(device, "luminance")->valueint);
    EXPECT_EQ(9, cJSON_GetObjectItem(device, "hue")->valueint);
    EXPECT_EQ(11, cJSON_GetObjectItem(device, "saturation")->valueint);
    EXPECT_EQ(12, cJSON_GetObjectItem(device, "contrast")->valueint);
    EXPECT_EQ(0, cJSON_GetObjectItem(device, "audio_fix")->valueint);
    EXPECT_EQ(3, cJSON_GetObjectItem(device, "vibration")->valueint);
    EXPECT_EQ(5, cJSON_GetObjectItem(device, "pwm_frequency")->valueint);
    cJSON *interface = cJSON_GetObjectItem(root_json, "interface");
    EXPECT_STREQ("fr.lang", cJSON_GetObjectItem(interface, "language")->valuestring);
    EXPECT_STREQ("/Themes/Bloom/", cJSON_GetObjectItem(interface, "theme")->valuestring);
    EXPECT_EQ(32, cJSON_GetObjectItem(interface, "font_size")->valueint);
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(interface, "background_music_muted")));
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(interface, "show_recents")));
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(interface, "show_expert")));
    cJSON *blue_light = cJSON_GetObjectItem(interface, "blue_light");
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(blue_light, "enabled")));
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(blue_light, "scheduled")));
    EXPECT_EQ(4, cJSON_GetObjectItem(blue_light, "level")->valueint);
    EXPECT_EQ(1122867, cJSON_GetObjectItem(blue_light, "rgb")->valueint);
    EXPECT_STREQ("21:30", cJSON_GetObjectItem(blue_light, "start_time")->valuestring);
    EXPECT_STREQ("07:15", cJSON_GetObjectItem(blue_light, "end_time")->valuestring);
    cJSON *recording = cJSON_GetObjectItem(interface, "recording");
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(recording, "indicator")));
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(recording, "hotkey")));
    EXPECT_EQ(4, cJSON_GetObjectItem(recording, "countdown")->valueint);
    cJSON *behavior = cJSON_GetObjectItem(root_json, "behavior");
    EXPECT_EQ(17, cJSON_GetObjectItem(behavior, "low_battery_warn_at")->valueint);
    EXPECT_EQ(6, cJSON_GetObjectItem(behavior, "low_battery_autosave_at")->valueint);
    EXPECT_EQ(3, cJSON_GetObjectItem(behavior, "startup_tab")->valueint);
    EXPECT_EQ(2, cJSON_GetObjectItem(behavior, "startup_application")->valueint);
    EXPECT_EQ(7, cJSON_GetObjectItem(behavior, "time_skip_hours")->valueint);
    cJSON *controls = cJSON_GetObjectItem(root_json, "controls");
    EXPECT_STREQ("CUSTOM", cJSON_GetObjectItem(controls, "layout")->valuestring);
    EXPECT_EQ(2, cJSON_GetObjectItem(controls, "mainui_single_press")->valueint);
    EXPECT_EQ(4, cJSON_GetObjectItem(controls, "mainui_long_press")->valueint);
    EXPECT_EQ(5, cJSON_GetObjectItem(controls, "mainui_double_press")->valueint);
    EXPECT_EQ(6, cJSON_GetObjectItem(controls, "ingame_single_press")->valueint);
    EXPECT_EQ(7, cJSON_GetObjectItem(controls, "ingame_long_press")->valueint);
    EXPECT_EQ(8, cJSON_GetObjectItem(controls, "ingame_double_press")->valueint);
    EXPECT_STREQ("Search", cJSON_GetObjectItem(controls, "mainui_button_x")->valuestring);
    EXPECT_STREQ("Activity", cJSON_GetObjectItem(controls, "mainui_button_y")->valuestring);
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
    EXPECT_EQ(20, cJSON_GetObjectItem(device, "background_music_volume")->valueint);
    EXPECT_FALSE(cJSON_IsTrue(cJSON_GetObjectItem(device, "mute")));
    cJSON *interface = cJSON_GetObjectItem(root_json, "interface");
    EXPECT_STREQ("/mnt/SDCARD/Themes/Silky by DiMo/",
                 cJSON_GetObjectItem(interface, "theme")->valuestring);
    EXPECT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(interface, "blue_light")));
    EXPECT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(interface, "recording")));
    cJSON *controls = cJSON_GetObjectItem(root_json, "controls");
    EXPECT_STREQ("L2,L,R2,R,X,A,B,Y", cJSON_GetObjectItem(controls, "layout")->valuestring);
    EXPECT_EQ(1, cJSON_GetObjectItem(controls, "mainui_single_press")->valueint);
    EXPECT_EQ(1, cJSON_GetObjectItem(controls, "ingame_single_press")->valueint);
    EXPECT_EQ(2, cJSON_GetObjectItem(controls, "ingame_long_press")->valueint);
    cJSON_Delete(root_json);
}

TEST_F(BloomSettingsTest, ImportUsesEffectiveLegacyFlagsInsteadOfDormantSystemFields)
{
    write(system, R"({"mute":1,"theme":"./"})");
    write(config / ".noBatteryWarning", "");
    write(config / ".noLowBatteryAutoSave", "");
    write(config / ".noVibration", "");
    write(config / ".menuInverted", "");
    write(config / ".noGameSwitcher", "");
    BloomSettingsImportResult result{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)))
        << error;
    cJSON *canonical = load_settings();
    cJSON *device = cJSON_GetObjectItem(canonical, "device");
    EXPECT_FALSE(cJSON_IsTrue(cJSON_GetObjectItem(device, "mute")));
    EXPECT_EQ(0, cJSON_GetObjectItem(device, "vibration")->valueint);
    cJSON *interface = cJSON_GetObjectItem(canonical, "interface");
    EXPECT_STREQ("/mnt/SDCARD/Themes/Silky by DiMo/",
                 cJSON_GetObjectItem(interface, "theme")->valuestring);
    cJSON *behavior = cJSON_GetObjectItem(canonical, "behavior");
    EXPECT_EQ(0, cJSON_GetObjectItem(behavior, "low_battery_warn_at")->valueint);
    EXPECT_EQ(0, cJSON_GetObjectItem(behavior, "low_battery_autosave_at")->valueint);
    cJSON *controls = cJSON_GetObjectItem(canonical, "controls");
    EXPECT_EQ(0, cJSON_GetObjectItem(controls, "mainui_single_press")->valueint);
    EXPECT_EQ(2, cJSON_GetObjectItem(controls, "ingame_single_press")->valueint);
    EXPECT_EQ(0, cJSON_GetObjectItem(controls, "ingame_long_press")->valueint);
    cJSON_Delete(canonical);
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

TEST_F(BloomSettingsTest, LegacySyncChangesOnlyWhenEffectiveInputChanges)
{
    write(system, R"({"vol":5,"unknown_legacy":"first"})");
    BloomSettingsImportResult imported{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &imported, error, sizeof(error)));

    BloomSettingsSyncResult unchanged{};
    ASSERT_EQ(0, bloom_settings_sync_onion(system.c_str(), config.c_str(), settings.c_str(),
                                           &unchanged, error, sizeof(error)))
        << error;
    EXPECT_EQ(0, unchanged.changed);
    EXPECT_EQ(1, unchanged.generation);

    cJSON *canonical = load_settings();
    ASSERT_TRUE(cJSON_IsObject(canonical));
    cJSON_AddStringToObject(canonical, "future_bloom_field", "preserved");
    char *serialized = cJSON_PrintUnformatted(canonical);
    ASSERT_NE(nullptr, serialized);
    write(settings, serialized);
    cJSON_free(serialized);
    cJSON_Delete(canonical);
    write(system, R"({"vol":16,"unknown_legacy":"second"})");
    write(config / ".showExpert", "");

    BloomSettingsSyncResult changed{};
    ASSERT_EQ(0, bloom_settings_sync_onion(system.c_str(), config.c_str(), settings.c_str(),
                                           &changed, error, sizeof(error)))
        << error;
    EXPECT_EQ(1, changed.changed);
    EXPECT_EQ(2, changed.generation);
    canonical = load_settings();
    EXPECT_EQ(2, cJSON_GetObjectItem(canonical, "generation")->valueint);
    EXPECT_STREQ("preserved", cJSON_GetObjectItem(canonical, "future_bloom_field")->valuestring);
    cJSON *device = cJSON_GetObjectItem(canonical, "device");
    EXPECT_EQ(16, cJSON_GetObjectItem(device, "volume")->valueint);
    cJSON *interface = cJSON_GetObjectItem(canonical, "interface");
    EXPECT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(interface, "show_expert")));
    cJSON *compatibility = cJSON_GetObjectItem(canonical, "compatibility");
    cJSON *legacy = cJSON_GetObjectItem(compatibility, "onion_system");
    EXPECT_STREQ("second", cJSON_GetObjectItem(legacy, "unknown_legacy")->valuestring);
    cJSON_Delete(canonical);

    BloomSettingsSyncResult repeated{};
    ASSERT_EQ(0, bloom_settings_sync_onion(system.c_str(), config.c_str(), settings.c_str(),
                                           &repeated, error, sizeof(error)));
    EXPECT_EQ(0, repeated.changed);
    EXPECT_EQ(2, repeated.generation);
}

TEST_F(BloomSettingsTest, LegacySyncAddsCompleteControlsToEarlierSchemaOneState)
{
    write(system, R"({"vol":5})");
    BloomSettingsImportResult imported{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &imported, error, sizeof(error)));
    cJSON *canonical = load_settings();
    cJSON_DeleteItemFromObjectCaseSensitive(canonical, "controls");
    char *serialized = cJSON_PrintUnformatted(canonical);
    ASSERT_NE(nullptr, serialized);
    write(settings, serialized);
    cJSON_free(serialized);
    cJSON_Delete(canonical);

    BloomSettingsSyncResult result{};
    ASSERT_EQ(0, bloom_settings_sync_onion(system.c_str(), config.c_str(), settings.c_str(),
                                           &result, error, sizeof(error)))
        << error;
    EXPECT_EQ(1, result.changed);
    EXPECT_EQ(2, result.generation);
    canonical = load_settings();
    EXPECT_TRUE(cJSON_IsObject(cJSON_GetObjectItem(canonical, "controls")));
    cJSON_Delete(canonical);
}

TEST_F(BloomSettingsTest, LegacySyncIsRejectedAfterBloomAuthorityCutover)
{
    write(system, R"({"vol":5})");
    BloomSettingsImportResult imported{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &imported, error, sizeof(error)));
    cJSON *canonical = load_settings();
    ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(
        canonical, "authority", cJSON_CreateString("bloom")));
    char *serialized = cJSON_PrintUnformatted(canonical);
    ASSERT_NE(nullptr, serialized);
    write(settings, serialized);
    cJSON_free(serialized);
    cJSON_Delete(canonical);
    write(system, R"({"vol":19})");

    BloomSettingsSyncResult result{};
    EXPECT_NE(0, bloom_settings_sync_onion(system.c_str(), config.c_str(), settings.c_str(),
                                           &result, error, sizeof(error)));
    canonical = load_settings();
    cJSON *device = cJSON_GetObjectItem(canonical, "device");
    EXPECT_EQ(5, cJSON_GetObjectItem(device, "volume")->valueint);
    EXPECT_EQ(1, cJSON_GetObjectItem(canonical, "generation")->valueint);
    cJSON_Delete(canonical);
}

TEST_F(BloomSettingsTest, ConcurrentLegacySyncsSerializeToOneGeneration)
{
    write(system, R"({"vol":5})");
    BloomSettingsImportResult imported{};
    char error[160] = {};
    ASSERT_EQ(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &imported, error, sizeof(error)));
    write(system, R"({"vol":13})");

    constexpr int process_count = 4;
    pid_t children[process_count] = {};
    for (int index = 0; index < process_count; ++index) {
        children[index] = fork();
        ASSERT_GE(children[index], 0);
        if (children[index] == 0) {
            BloomSettingsSyncResult result{};
            char child_error[160] = {};
            int status = bloom_settings_sync_onion(system.c_str(), config.c_str(), settings.c_str(),
                                                   &result, child_error, sizeof(child_error));
            _exit(status == 0 ? 0 : 1);
        }
    }
    for (pid_t child : children) {
        int status = 0;
        ASSERT_EQ(child, waitpid(child, &status, 0));
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(0, WEXITSTATUS(status));
    }
    cJSON *canonical = load_settings();
    ASSERT_TRUE(cJSON_IsObject(canonical));
    EXPECT_EQ(2, cJSON_GetObjectItem(canonical, "generation")->valueint);
    cJSON *device = cJSON_GetObjectItem(canonical, "device");
    EXPECT_EQ(13, cJSON_GetObjectItem(device, "volume")->valueint);
    cJSON_Delete(canonical);
}

TEST_F(BloomSettingsTest, SymlinkedWriterLockFailsClosed)
{
    auto outside = root / "outside.lock";
    write(outside, "outside");
    std::filesystem::create_symlink(outside, settings.string() + ".lock");
    write(system, R"({"vol":8})");
    BloomSettingsImportResult result{};
    char error[160] = {};
    EXPECT_NE(0, bloom_settings_import_onion(system.c_str(), config.c_str(), settings.c_str(),
                                             snapshot.c_str(), &result, error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(settings));
}
