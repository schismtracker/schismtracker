#include "headers.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mixer.h"
#include "util.h"

/* --------------------------------------------------------------------- */

static const char *device_file = NULL;

struct stereo_volume {
        byte left, right;
};

/* --------------------------------------------------------------------- */

static int open_mixer_device(void)
{
        const char *ptr;

        if (!device_file) {
                ptr = "/dev/sound/mixer";
                if (!access(ptr, F_OK)) {
                        /* this had better work :) */
                        ptr = "/dev/mixer";
                }
                device_file = ptr;
        }

        return open(device_file, O_RDWR);
}

/* --------------------------------------------------------------------- */

void mixer_read_master_volume(int *left, int *right)
{
        int fd;
        struct stereo_volume volume;

        fd = open_mixer_device();
        if (fd < 0) {
                perror(device_file);
                *left = *right = 0;
                return;
        }

        if (ioctl(fd, SOUND_MIXER_READ_VOLUME, &volume) == EOF) {
                perror(device_file);
                *left = *right = 0;
        } else {
                *left = volume.left;
                *right = volume.right;
        }

        close(fd);
}

void mixer_write_master_volume(int left, int right)
{
        int fd;
        struct stereo_volume volume = {
                CLAMP(left, 0, VOLUME_MAX),
                CLAMP(right, 0, VOLUME_MAX)
        };

        fd = open_mixer_device();
        if (fd < 0) {
                perror(device_file);
                return;
        }

        if (ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &volume) == EOF) {
                perror(device_file);
        }

        close(fd);
}
