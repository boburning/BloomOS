#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "database/sqlite_config.h"
}

namespace {

class SqliteConfigTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        path_ = std::filesystem::temp_directory_path() /
                ("bloom-sqlite-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 ".sqlite");
    }

    void TearDown() override
    {
        std::filesystem::remove(path_);
        std::filesystem::remove(path_.string() + "-wal");
        std::filesystem::remove(path_.string() + "-shm");
    }

    sqlite3 *open_configured()
    {
        sqlite3 *db = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_open(path_.string().c_str(), &db));
        EXPECT_NE(nullptr, db);
        EXPECT_EQ(SQLITE_OK, bloom_sqlite_configure(db));
        return db;
    }

    int pragma_int(sqlite3 *db, const char *sql)
    {
        sqlite3_stmt *stmt = nullptr;
        EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr));
        EXPECT_EQ(SQLITE_ROW, sqlite3_step(stmt));
        int value = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return value;
    }

    std::filesystem::path path_;
};

TEST_F(SqliteConfigTest, EnablesDurableWalAndBusyTimeout)
{
    sqlite3 *db = open_configured();
    sqlite3_stmt *stmt = nullptr;

    ASSERT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, "PRAGMA journal_mode;", -1, &stmt, nullptr));
    ASSERT_EQ(SQLITE_ROW, sqlite3_step(stmt));
    EXPECT_STREQ("wal", reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
    sqlite3_finalize(stmt);

    EXPECT_EQ(BLOOM_SQLITE_BUSY_TIMEOUT_MS, pragma_int(db, "PRAGMA busy_timeout;"));
    EXPECT_EQ(2, pragma_int(db, "PRAGMA synchronous;"));
    EXPECT_EQ(SQLITE_OK, sqlite3_close(db));
}

TEST_F(SqliteConfigTest, WaitsForConcurrentWriterInsteadOfFailingImmediately)
{
    sqlite3 *writer = open_configured();
    sqlite3 *contender = open_configured();
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(writer, "CREATE TABLE activity(id INTEGER);", nullptr, nullptr, nullptr));
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(writer, "BEGIN IMMEDIATE; INSERT INTO activity VALUES(1);", nullptr, nullptr, nullptr));
    ASSERT_EQ(SQLITE_OK, sqlite3_busy_timeout(contender, 100));

    auto started = std::chrono::steady_clock::now();
    int rc = sqlite3_exec(contender, "INSERT INTO activity VALUES(2);", nullptr, nullptr, nullptr);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

    EXPECT_EQ(SQLITE_BUSY, rc);
    EXPECT_GE(elapsed.count(), 80);
    EXPECT_EQ(SQLITE_OK, sqlite3_exec(writer, "ROLLBACK;", nullptr, nullptr, nullptr));
    EXPECT_EQ(SQLITE_OK, sqlite3_close(contender));
    EXPECT_EQ(SQLITE_OK, sqlite3_close(writer));
}

TEST_F(SqliteConfigTest, CheckpointsAndRemovesSidecarsAfterLastClose)
{
    sqlite3 *db = open_configured();
    ASSERT_EQ(SQLITE_OK,
              sqlite3_exec(db, "CREATE TABLE activity(id INTEGER); INSERT INTO activity VALUES(1);", nullptr, nullptr, nullptr));
    ASSERT_TRUE(std::filesystem::exists(path_.string() + "-wal"));
    EXPECT_EQ(SQLITE_OK, sqlite3_close(db));

    EXPECT_FALSE(std::filesystem::exists(path_.string() + "-wal"));
    EXPECT_FALSE(std::filesystem::exists(path_.string() + "-shm"));

    sqlite3 *reopened = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path_.string().c_str(), &reopened));
    EXPECT_EQ(1, pragma_int(reopened, "SELECT COUNT(*) FROM activity;"));
    EXPECT_EQ(SQLITE_OK, sqlite3_close(reopened));
}

TEST_F(SqliteConfigTest, RecoversCommittedWalAfterAbruptWriterExit)
{
    pid_t writer = fork();
    ASSERT_NE(-1, writer);
    if (writer == 0) {
        sqlite3 *db = nullptr;
        int rc = sqlite3_open(path_.string().c_str(), &db);
        if (rc == SQLITE_OK)
            rc = bloom_sqlite_configure(db);
        if (rc == SQLITE_OK)
            rc = sqlite3_exec(db, "CREATE TABLE activity(id INTEGER); INSERT INTO activity VALUES(1);", nullptr,
                              nullptr, nullptr);
        _exit(rc == SQLITE_OK ? 0 : 1);
    }

    int status = 0;
    ASSERT_EQ(writer, waitpid(writer, &status, 0));
    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(0, WEXITSTATUS(status));
    ASSERT_TRUE(std::filesystem::exists(path_.string() + "-wal"));

    sqlite3 *recovered = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path_.string().c_str(), &recovered));
    EXPECT_EQ(1, pragma_int(recovered, "SELECT COUNT(*) FROM activity;"));
    EXPECT_EQ(SQLITE_OK, sqlite3_close(recovered));
}

} // namespace
