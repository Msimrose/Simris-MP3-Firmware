/* POSIX tree walk, isolated from FatFs headers (DIR typedef collision). */
#pragma once

typedef void (*host_walk_cb)(const char *abs_path, const char *rel_path,
                             int is_dir, void *user);

/* Walks root recursively (dirs before contents, alphabetical), calling cb
 * for every directory and regular file. Returns 0, or -1 if root is
 * unreadable. */
int host_walk(const char *root, host_walk_cb cb, void *user);
