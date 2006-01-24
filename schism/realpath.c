#include <string.h>

/* This is a mess that only kind of works. */
char *realpath(const char *path, char *resolved_path);
char *realpath(const char *path, char *resolved_path)
{
	strcpy(resolved_path, path);
	return resolved_path;
}
