/* POSIX-side tree walk for fatfs_test.
 *
 * Lives in its own translation unit because <dirent.h>'s DIR typedef
 * collides with FatFs' DIR - the FatFs side of the test must never see
 * this header. Directories are reported before their contents; entries
 * are visited in alphabetical order for deterministic output. */
#include "host_walk.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int walk(const char *abs, const char *rel, host_walk_cb cb, void *user,
                int depth)
{
    if (depth > 12) return 0;

    struct dirent **ents = NULL;
    int n = scandir(abs, &ents, NULL, alphasort);
    if (n < 0) return -1;

    for (int i = 0; i < n; i++) {
        const char *name = ents[i]->d_name;
        if (name[0] == '.') { free(ents[i]); continue; }

        char a[1024], r[1024];
        snprintf(a, sizeof a, "%s/%s", abs, name);
        if (rel[0]) snprintf(r, sizeof r, "%s/%s", rel, name);
        else        snprintf(r, sizeof r, "%s", name);

        struct stat st;
        if (stat(a, &st) == 0) {              /* follows symlinks */
            if (S_ISDIR(st.st_mode)) {
                cb(a, r, 1, user);
                walk(a, r, cb, user, depth + 1);
            } else if (S_ISREG(st.st_mode)) {
                cb(a, r, 0, user);
            }
        }
        free(ents[i]);
    }
    free(ents);
    return 0;
}

int host_walk(const char *root, host_walk_cb cb, void *user)
{
    return walk(root, "", cb, user, 0);
}
