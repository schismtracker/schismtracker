/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"

#include "mixer.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <errno.h>
#include <fcntl.h>

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
                if (access(ptr, F_OK) < 0) {
                        /* this had better work :) */
                        ptr = "/dev/mixer";
                }
                device_file = ptr;
        }

        return open(device_file, O_RDWR);
}

/* --------------------------------------------------------------------- */

void mixer_read_volume(int *left, int *right)
{
        int fd;
        struct stereo_volume volume;

        fd = open_mixer_device();
        if (fd < 0) {
                perror(device_file);
                *left = *right = 0;
                return;
        }

        if (ioctl(fd, MIXER_READ(SCHISM_MIXER_CONTROL), &volume) == EOF) {
                perror(device_file);
                *left = *right = 0;
        } else {
                *left = volume.left;
                *right = volume.right;
        }

        close(fd);
}

void mixer_write_volume(int left, int right)
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

        if (ioctl(fd, MIXER_WRITE(SCHISM_MIXER_CONTROL), &volume) == EOF) {
                perror(device_file);
        }

        close(fd);
}
