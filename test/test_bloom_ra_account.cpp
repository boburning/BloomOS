#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sys/stat.h>

extern "C" {
#include "../src/bloomRa/bloom_ra_account.h"
}

class BloomRaAccountTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = std::filesystem::temp_directory_path() / ("bloom-ra-account-" + std::to_string(getpid()));
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        settings = root / "account.json";
        credentials = root / "credentials";
    }
    void TearDown() override { std::filesystem::remove_all(root); }
    std::filesystem::path root, settings, credentials;
};

TEST_F(BloomRaAccountTest, DefaultsToDisabledWithoutPersistedSecrets)
{
    BloomRaAccountStatus status = {};
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_account_load(settings.c_str(), credentials.c_str(), &status, error, sizeof(error)));
    EXPECT_EQ(1, status.schema);
    EXPECT_EQ(0, status.enabled);
    EXPECT_EQ(0, status.authenticated);
    EXPECT_STREQ("softcore", status.mode);
}

TEST_F(BloomRaAccountTest, StoresTokenSeparatelyWithRestrictivePermissionsAndRedactedStatus)
{
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_account_store(settings.c_str(), credentials.c_str(), "BloomUser", "secret-token", 1,
                                        "softcore", 1, error, sizeof(error)))
        << error;
    struct stat metadata = {};
    ASSERT_EQ(0, stat(credentials.c_str(), &metadata));
    EXPECT_EQ(0, metadata.st_mode & 077);
    std::ifstream input(settings);
    std::string public_settings((std::istreambuf_iterator<char>(input)), {});
    EXPECT_EQ(std::string::npos, public_settings.find("secret-token"));
    BloomRaAccountStatus status = {};
    ASSERT_EQ(0, bloom_ra_account_load(settings.c_str(), credentials.c_str(), &status, error, sizeof(error)));
    EXPECT_EQ(1, status.authenticated);
    EXPECT_STREQ("BloomUser", status.username);
    EXPECT_STREQ("softcore", status.mode);
    EXPECT_EQ(1, status.offline_casual);
}

TEST_F(BloomRaAccountTest, RejectsHardcoreProxyCombinationAndUnsafeCredentialFile)
{
    char error[128] = {};
    EXPECT_NE(0, bloom_ra_account_store(settings.c_str(), credentials.c_str(), "BloomUser", "token", 1,
                                        "hardcore", 1, error, sizeof(error)));
    std::ofstream(credentials) << "token";
    chmod(credentials.c_str(), 0644);
    char token[128] = {};
    EXPECT_NE(0, bloom_ra_account_read_token(credentials.c_str(), token, sizeof(token)));
}

TEST_F(BloomRaAccountTest, AllowsPublicNonSecretSettingsPermissions)
{
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_account_store(settings.c_str(), credentials.c_str(), "BloomUser", "token", 1,
                                        "softcore", 0, error, sizeof(error)));
    ASSERT_EQ(0, chmod(settings.c_str(), 0644));
    BloomRaAccountStatus status = {};
    ASSERT_EQ(0, bloom_ra_account_load(settings.c_str(), credentials.c_str(), &status, error, sizeof(error)));
    EXPECT_EQ(1, status.authenticated);
    EXPECT_STREQ("BloomUser", status.username);
}

TEST_F(BloomRaAccountTest, SignOutRemovesSettingsAndCredential)
{
    char error[128] = {};
    ASSERT_EQ(0, bloom_ra_account_store(settings.c_str(), credentials.c_str(), "BloomUser", "token", 1,
                                        "softcore", 0, error, sizeof(error)));
    ASSERT_EQ(0, bloom_ra_account_sign_out(settings.c_str(), credentials.c_str(), error, sizeof(error)));
    EXPECT_FALSE(std::filesystem::exists(settings));
    EXPECT_FALSE(std::filesystem::exists(credentials));
}
