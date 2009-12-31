/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
 * URL: http://schismtracker.org/
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

#ifdef USE_OSS

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <errno.h>
#include <fcntl.h>

/* Hmm. I've found that some systems actually don't support the idea of a
 * "master" volume, so this became necessary. I suppose PCM is really the
 * more useful setting to change anyway. */
#if 0
# define SCHISM_MIXER_CONTROL SOUND_MIXER_VOLUME
#else
# define SCHISM_MIXER_CONTROL SOUND_MIXER_PCM
#endif

#define VOLUME_MAX 100

/* --------------------------------------------------------------------- */

static const char *device_file = NULL;

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

int oss_mixer_get_max_volume(void);
int oss_mixer_get_max_volume(void)
{
        return VOLUME_MAX;
}

void oss_mixer_read_volume(int *left, int *right);
void oss_mixer_read_volume(int *left, int *right)
{
        int fd;
        uint8_t volume[4];

        fd = open_mixer_device();
        if (fd < 0) {
                perror(device_file);
                *left = *right = 0;
                return;
        }

        if (ioctl(fd, MIXER_READ(SCHISM_MIXER_CONTROL), volume) == EOF) {
                perror(device_file);
                *left = *right = 0;
        } else {
                *left = volume[0];
                *right = volume[1];
        }

        close(fd);
}

void oss_mixer_write_volume(int left, int right);
void oss_mixer_write_volume(int left, int right)
{
        int fd;
        uint8_t volume[4];

        volume[0] = CLAMP(left, 0, VOLUME_MAX);
        volume[1] = CLAMP(right, 0, VOLUME_MAX);

        fd = open_mixer_device();
        if (fd < 0) {
                perror(device_file);
                return;
        }

        if (ioctl(fd, MIXER_WRITE(SCHISM_MIXER_CONTROL), volume) == EOF) {
                perror(device_file);
        }

        close(fd);
}

#endif
