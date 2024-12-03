/*
 * "dirent.h" for the Macintosh.
 * Public domain by Guido van Rossum, CWI, Amsterdam (July 1987).
 *
 * Edited for use in Schism Tracker by Paper, 2024.
 */

#ifndef SCHISM_SYS_MACOS_DIRENT_H_
#define SCHISM_SYS_MACOS_DIRENT_H_

typedef struct dir_ DIR;

struct dirent {
	char d_name[256];
};

extern DIR *opendir(const char *path);
extern struct dirent *readdir(DIR *dirp);
extern void closedir(DIR *dirp);

#endif /* SCHISM_SYS_MACOS_DIRENT_H_ */
