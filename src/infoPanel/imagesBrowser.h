#ifndef IMAGES_BROWSER_H__
#define IMAGES_BROWSER_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool loadImagesPathsFromDir(const char *dir_path, char ***images_paths,
                            int *images_paths_count);

#ifdef __cplusplus
}
#endif

#endif // IMAGES_BROWSER_H__
