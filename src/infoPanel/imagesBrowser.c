#include "imagesBrowser.h"

#include <ctype.h>
#include <dirent.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STR_MAX 256

static const char *getFilenameExt(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

static char *toLower(char *s)
{
    for (char *p = s; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    return s;
}

static bool getImagePath(const char *dir_path, const struct dirent *ent,
                         char *image_path)
{
    const int ext_size = 50;
    char ext[ext_size];
    const char *filename = ent->d_name;
    if (filename[0] == '.') {
        return false;
    }
    if (snprintf(ext, sizeof(ext), "%s", getFilenameExt(filename)) >= ext_size) {
        return false;
    }
    const char *fileExt = toLower(ext);
    if (strcmp(fileExt, "png") == 0 || strcmp(fileExt, "jpg") == 0 ||
        strcmp(fileExt, "jpeg") == 0) {
        if (snprintf(image_path, PATH_MAX, "%s%s", dir_path, filename) >= PATH_MAX) {
            return false;
        }
        struct stat path_stat;
        if (stat(image_path, &path_stat) != 0 || !S_ISREG(path_stat.st_mode)) {
            return false;
        }
        return true;
    }
    return false;
}

static void freeImagesPaths(char **images_paths, int images_paths_count)
{
    for (int i = 0; i < images_paths_count; i++) {
        free(images_paths[i]);
    }
    free(images_paths);
}

int compare_strings(const void *a, const void *b)
{
    const char *aa = *(const char **)a;
    const char *bb = *(const char **)b;
    return strcmp(aa, bb);
}

bool loadImagesPathsFromDir(const char *dir_path, char ***images_paths,
                            int *images_paths_count)
{
    if (dir_path == NULL || images_paths == NULL || images_paths_count == NULL) {
        return false;
    }

    *images_paths = NULL;
    *images_paths_count = 0;

    char normalized_dir_path[PATH_MAX];
    const int dir_path_length = strlen(dir_path);
    if (dir_path_length == 0) {
        return false;
    }
    if (dir_path[dir_path_length - 1] != '/') {
        if (snprintf(normalized_dir_path, sizeof(normalized_dir_path), "%s/", dir_path) >= PATH_MAX) {
            return false;
        }
    }
    else {
        if (snprintf(normalized_dir_path, sizeof(normalized_dir_path), "%s", dir_path) >= PATH_MAX) {
            return false;
        }
    }

    DIR *dir = opendir(normalized_dir_path);

    if (dir == NULL) {
        return false;
    }

    struct dirent *ent;

    int images_capacity = 0;

    while ((ent = readdir(dir)) != NULL) {
        char image_path[PATH_MAX];
        const bool is_image =
            getImagePath(normalized_dir_path, ent, image_path);

        if (!is_image) {
            continue;
        }

        if (*images_paths_count == images_capacity) {
            const int new_capacity = images_capacity == 0 ? 8 : images_capacity * 2;
            char **resized = (char **)realloc(
                *images_paths, (size_t)new_capacity * sizeof(char *));
            if (resized == NULL) {
                freeImagesPaths(*images_paths, *images_paths_count);
                *images_paths = NULL;
                *images_paths_count = 0;
                closedir(dir);
                return false;
            }
            *images_paths = resized;
            images_capacity = new_capacity;
        }

        const size_t image_path_size = strlen(image_path) + 1;
        (*images_paths)[*images_paths_count] = (char *)malloc(image_path_size);
        if ((*images_paths)[*images_paths_count] == NULL) {
            freeImagesPaths(*images_paths, *images_paths_count);
            *images_paths = NULL;
            *images_paths_count = 0;
            closedir(dir);
            return false;
        }
        memcpy((*images_paths)[*images_paths_count], image_path, image_path_size);
        (*images_paths_count)++;
    }

    closedir(dir);

    if (*images_paths_count > 1) {
        qsort(*images_paths, *images_paths_count, sizeof(char *), compare_strings);
    }

    return true;
}
