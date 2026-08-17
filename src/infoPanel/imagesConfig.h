#ifndef IMAGES_CONFIG_H__
#define IMAGES_CONFIG_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool loadImagesPathsFromJson(const char *config_path, char ***images_paths,
                             int *images_paths_count, char ***images_titles);
void freeImagesConfig(char **images_paths, char **images_titles,
                      int images_paths_count);

#ifdef __cplusplus
}
#endif

#endif // IMAGES_CONFIG_H__
