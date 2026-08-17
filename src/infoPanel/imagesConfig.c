#include "imagesConfig.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"
#include "utils/file.h"

void freeImagesConfig(char **images_paths, char **images_titles,
                      int images_paths_count)
{
    for (int i = 0; i < images_paths_count; i++) {
        free(images_paths == NULL ? NULL : images_paths[i]);
        free(images_titles == NULL ? NULL : images_titles[i]);
    }
    free(images_paths);
    free(images_titles);
}

static bool appendImage(const char *directory, const cJSON *item,
                        char **image_path, char **image_title)
{
    const cJSON *path_item = cJSON_GetObjectItemCaseSensitive(item, "path");
    if (!cJSON_IsString(path_item) || path_item->valuestring == NULL ||
        path_item->valuestring[0] == '\0') {
        return false;
    }

    char full_path[PATH_MAX];
    const int path_size = snprintf(full_path, sizeof(full_path), "%s/%s",
                                   directory, path_item->valuestring);
    if (path_size < 0 || path_size >= PATH_MAX) {
        return false;
    }
    *image_path = (char *)malloc((size_t)path_size + 1);
    if (*image_path == NULL) {
        return false;
    }
    memcpy(*image_path, full_path, (size_t)path_size + 1);

    const cJSON *title_item = cJSON_GetObjectItemCaseSensitive(item, "title");
    if (cJSON_IsString(title_item) && title_item->valuestring != NULL) {
        const size_t title_size = strlen(title_item->valuestring) + 1;
        *image_title = (char *)malloc(title_size);
        if (*image_title == NULL) {
            free(*image_path);
            *image_path = NULL;
            return false;
        }
        memcpy(*image_title, title_item->valuestring, title_size);
    }
    return true;
}

bool loadImagesPathsFromJson(const char *config_path, char ***images_paths,
                             int *images_paths_count, char ***images_titles)
{
    if (config_path == NULL || images_paths == NULL ||
        images_paths_count == NULL || images_titles == NULL) {
        return false;
    }
    *images_paths = NULL;
    *images_titles = NULL;
    *images_paths_count = 0;

    char *json_text = file_read(config_path);
    if (json_text == NULL) {
        return false;
    }
    cJSON *root = cJSON_Parse(json_text);
    free(json_text);
    if (root == NULL) {
        return false;
    }

    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "images");
    if (!cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return false;
    }
    char *directory = file_dirname(config_path);
    if (directory == NULL) {
        directory = (char *)malloc(2);
        if (directory != NULL) {
            memcpy(directory, ".", 2);
        }
    }
    if (directory == NULL) {
        cJSON_Delete(root);
        return false;
    }

    const int item_count = cJSON_GetArraySize(items);
    char **paths = item_count == 0
                       ? NULL
                       : (char **)calloc((size_t)item_count, sizeof(char *));
    char **titles = item_count == 0
                        ? NULL
                        : (char **)calloc((size_t)item_count, sizeof(char *));
    if (item_count > 0 && (paths == NULL || titles == NULL)) {
        free(paths);
        free(titles);
        free(directory);
        cJSON_Delete(root);
        return false;
    }

    int valid_count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, items)
    {
        if (!cJSON_IsObject(item)) {
            continue;
        }
        if (appendImage(directory, item, &paths[valid_count],
                        &titles[valid_count])) {
            valid_count++;
        }
    }
    free(directory);
    cJSON_Delete(root);

    if (valid_count == 0) {
        free(paths);
        free(titles);
        return true;
    }
    *images_paths = paths;
    *images_titles = titles;
    *images_paths_count = valid_count;
    return true;
}
