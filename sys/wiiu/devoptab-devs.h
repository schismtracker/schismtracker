#ifndef DEVICE
# define DEVICE(id, name, path, sdcard)
#endif
/* numeric ID should start at zero and count up.
 * it is used as an array indice :) */
DEVICE(0, "sd", "/dev/sdcard01", 1)
#undef DEVICE