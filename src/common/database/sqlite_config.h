#ifndef BLOOM_SQLITE_CONFIG_H
#define BLOOM_SQLITE_CONFIG_H

#include <stddef.h>
#include <sqlite3/sqlite3.h>

#define BLOOM_SQLITE_BUSY_TIMEOUT_MS 3000

static inline int bloom_sqlite_configure(sqlite3 *db)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (db == NULL)
        return SQLITE_MISUSE;

    rc = sqlite3_busy_timeout(db, BLOOM_SQLITE_BUSY_TIMEOUT_MS);
    if (rc != SQLITE_OK)
        return rc;

    rc = sqlite3_prepare_v2(db, "PRAGMA journal_mode=WAL;", -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return rc;

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *mode = sqlite3_column_text(stmt, 0);
        rc = mode != NULL && sqlite3_stricmp((const char *)mode, "wal") == 0 ? SQLITE_OK : SQLITE_ERROR;
    }
    else {
        rc = rc == SQLITE_DONE ? SQLITE_ERROR : rc;
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_OK)
        return rc;

    return sqlite3_exec(db, "PRAGMA synchronous=FULL;", NULL, NULL, NULL);
}

#endif
