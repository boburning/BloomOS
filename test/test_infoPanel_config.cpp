#include "gtest/gtest.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

#include "../src/infoPanel/imagesConfig.h"

static std::string writeConfig(const char *json)
{
    char directory_template[] = "/tmp/bloom-info-config-XXXXXX";
    char *directory = mkdtemp(directory_template);
    EXPECT_NE(directory, (char *)NULL);
    std::string path = std::string(directory) + "/info.json";
    std::ofstream(path) << json;
    return path;
}

static void removeConfig(const std::string &path)
{
    unlink(path.c_str());
    std::string directory = path.substr(0, path.rfind('/'));
    rmdir(directory.c_str());
}

TEST(test_infoPanelConfig, compactsValidEntriesAndKeepsTitlesAligned)
{
    const std::string config = writeConfig(
        "{\"images\":[{\"path\":\"one.png\",\"title\":\"One\"},"
        "{\"title\":\"missing path\"},{\"path\":7},"
        "{\"path\":\"two.jpg\"}]}");
    char **paths = NULL;
    char **titles = NULL;
    int count = -1;

    ASSERT_TRUE(loadImagesPathsFromJson(config.c_str(), &paths, &count, &titles));
    ASSERT_EQ(count, 2);
    EXPECT_EQ(std::string(paths[0]), config.substr(0, config.rfind('/')) + "/one.png");
    EXPECT_EQ(std::string(titles[0]), "One");
    EXPECT_EQ(std::string(paths[1]), config.substr(0, config.rfind('/')) + "/two.jpg");
    EXPECT_EQ(titles[1], (char *)NULL);

    freeImagesConfig(paths, titles, count);
    removeConfig(config);
}

TEST(test_infoPanelConfig, rejectsMalformedOrMissingImageArrays)
{
    for (const char *json : {"not json", "{}", "{\"images\":{}}"}) {
        const std::string config = writeConfig(json);
        char **paths = (char **)0x1;
        char **titles = (char **)0x1;
        int count = -1;
        EXPECT_FALSE(loadImagesPathsFromJson(config.c_str(), &paths, &count, &titles));
        EXPECT_EQ(paths, (char **)NULL);
        EXPECT_EQ(titles, (char **)NULL);
        EXPECT_EQ(count, 0);
        removeConfig(config);
    }
}

TEST(test_infoPanelConfig, acceptsAnEmptyImageArrayWithoutAllocating)
{
    const std::string config = writeConfig("{\"images\":[]}");
    char **paths = (char **)0x1;
    char **titles = (char **)0x1;
    int count = -1;

    EXPECT_TRUE(loadImagesPathsFromJson(config.c_str(), &paths, &count, &titles));
    EXPECT_EQ(paths, (char **)NULL);
    EXPECT_EQ(titles, (char **)NULL);
    EXPECT_EQ(count, 0);
    removeConfig(config);
}
