#include "gtest/gtest.h"

extern "C" {
#include "../src/bloomShell/bloom_shell_search.h"
}

TEST(BloomShellSearch, FiltersTitlesLocallyAndCaseInsensitively)
{
    BloomLibraryGame games[3] = {};
    snprintf(games[0].display_title, sizeof(games[0].display_title), "Advance Wars");
    snprintf(games[1].display_title, sizeof(games[1].display_title), "Astro Boy");
    snprintf(games[2].display_title, sizeof(games[2].display_title), "Golden Sun");
    BloomShellSearch search;
    ASSERT_EQ(0, bloom_shell_search_init(&search, 3));
    snprintf(search.query, sizeof(search.query), "aDVa");
    ASSERT_EQ(0, bloom_shell_search_rebuild(&search, games, 3, 8));
    ASSERT_EQ(1U, search.focus.item_count);
    EXPECT_STREQ("Advance Wars", search.results[0]->display_title);
    EXPECT_EQ(1, search.active);
    bloom_shell_search_destroy(&search);
}

TEST(BloomShellSearch, EmptyQueryRestoresAllRows)
{
    BloomLibraryGame games[2] = {};
    snprintf(games[0].display_title, sizeof(games[0].display_title), "One");
    snprintf(games[1].display_title, sizeof(games[1].display_title), "Two");
    BloomShellSearch search;
    ASSERT_EQ(0, bloom_shell_search_init(&search, 2));
    snprintf(search.query, sizeof(search.query), "missing");
    ASSERT_EQ(0, bloom_shell_search_rebuild(&search, games, 2, 8));
    EXPECT_EQ(0U, search.focus.item_count);
    search.query[0] = '\0';
    ASSERT_EQ(0, bloom_shell_search_rebuild(&search, games, 2, 8));
    EXPECT_EQ(2U, search.focus.item_count);
    EXPECT_EQ(0, search.active);
    bloom_shell_search_destroy(&search);
}
