#include "gtest/gtest.h"

#include <string>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL/SDL.h>

#include "../src/infoPanel/imagesCache.h"
#include "../src/infoPanel/imagesBrowser.h"

#define STR_MAX 256

TEST(test_infoPanel, imageDirectoryScanIsDynamicAndSorted)
{
    char directory_template[] = "/tmp/bloom-images-XXXXXX";
    char *directory = mkdtemp(directory_template);
    ASSERT_NE(directory, (char *)NULL);

    const char *filenames[] = {"z.JPG", "ignore.txt", "a.png", ".hidden.png", "m.jpeg"};
    for (const char *filename : filenames) {
        std::ofstream(std::string(directory) + "/" + filename) << "fixture";
    }
    ASSERT_EQ(mkdir((std::string(directory) + "/folder.png").c_str(), 0700), 0);

    char **images_paths = NULL;
    int images_paths_count = -1;
    ASSERT_TRUE(loadImagesPathsFromDir(directory, &images_paths, &images_paths_count));
    ASSERT_EQ(images_paths_count, 3);
    EXPECT_EQ(std::string(images_paths[0]), std::string(directory) + "/a.png");
    EXPECT_EQ(std::string(images_paths[1]), std::string(directory) + "/m.jpeg");
    EXPECT_EQ(std::string(images_paths[2]), std::string(directory) + "/z.JPG");

    for (int i = 0; i < images_paths_count; i++) {
        free(images_paths[i]);
    }
    free(images_paths);
    for (const char *filename : filenames) {
        unlink((std::string(directory) + "/" + filename).c_str());
    }
    rmdir((std::string(directory) + "/folder.png").c_str());
    rmdir(directory);
}

TEST(test_infoPanel, emptyImageDirectoryReturnsAnEmptyList)
{
    char directory_template[] = "/tmp/bloom-images-empty-XXXXXX";
    char *directory = mkdtemp(directory_template);
    ASSERT_NE(directory, (char *)NULL);

    char **images_paths = (char **)0x1;
    int images_paths_count = -1;
    EXPECT_TRUE(loadImagesPathsFromDir(directory, &images_paths, &images_paths_count));
    EXPECT_EQ(images_paths, (char **)NULL);
    EXPECT_EQ(images_paths_count, 0);
    rmdir(directory);
}

typedef struct
{
    int initial_index;
    int new_index;
    bool cache_used;
    std::string drawn_image_path;
} TestItem;

TEST(test_infoPanel, cacheTest)
{
    char **images_paths = NULL;
    int images_paths_count = 5;
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    ASSERT_NE(screen, (SDL_Surface*)NULL);

    const int test_data_count = 11;
    TestItem test_data[test_data_count];
    test_data[0] = { 0, -1, false, "" };
    test_data[1] = { 0, 0, false, "./infoPanel_test_data/page0.png" };
    test_data[2] = { 0, 1, true, "./infoPanel_test_data/page1.png" };
    test_data[3] = { 1, 1, true, "./infoPanel_test_data/page1.png" };
    test_data[4] = { 1, 2, true, "./infoPanel_test_data/page2.png" };
    test_data[5] = { 2, 3, true, "./infoPanel_test_data/page3.png" };
    test_data[6] = { 3, 2, true, "./infoPanel_test_data/page2.png" };
    test_data[7] = { 2, 4, false, "" }; // random jump is not yet implemented
    test_data[8] = { 2, 3, true, "./infoPanel_test_data/page3.png" };
    test_data[9] = { 3, 4, true, "./infoPanel_test_data/page4.png" };
    test_data[10] = { 4, 5, false, "" };

    images_paths = new char*[images_paths_count];

	for (int i = 0; i < images_paths_count; i++)
	{
		images_paths[i] = new char[STR_MAX];
		snprintf(images_paths[i], STR_MAX, "./infoPanel_test_data/page%d.png", i);
	}

    char* drawn_image_path = NULL;
    for (int i = 0; i < test_data_count; i++)
    {
        printf("Entering test item #%d\n", i);
        const TestItem& test_item = test_data[i];
        bool cache_used = false;
        drawn_image_path = drawImageByIndex(test_item.new_index, test_item.initial_index,
            images_paths, images_paths_count, screen, NULL, &cache_used);

        if (drawn_image_path != NULL)
        {
            ASSERT_STREQ(drawn_image_path, test_item.drawn_image_path.c_str());
        }
        else
        {
            ASSERT_EQ(test_item.drawn_image_path, "");
        }
        
        ASSERT_EQ(cache_used, test_item.cache_used);
    }

    for (int i = 0; i < images_paths_count; i++)
    {
        delete[] images_paths[i];
    }
    delete[] images_paths;
    cleanImagesCache();
    SDL_FreeSurface(screen);
}
