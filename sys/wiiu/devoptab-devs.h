#ifndef DEVICE
# define DEVICE(id, name, path, sdcard)
#endif
/* numeric ID should start at zero and count up.
 * it is used as an array indice :) */
DEVICE(0, "sd", "/dev/sdcard01", 1)
/* There are only four physical USB ports.
 * This is definitely enough */
DEVICE(1, "usb1", "/dev/usb01", 0)
DEVICE(2, "usb2", "/dev/usb02", 0)
DEVICE(3, "usb3", "/dev/usb03", 0)
DEVICE(4, "usb4", "/dev/usb04", 0)
DEVICE(5, "slccmpt", "/dev/slccmpt01", 0)
DEVICE(6, "slc", "/dev/slc01", 0)
#undef DEVICE