/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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

#include "it.h"
#include "song.h"
#include "page.h"
#include "util.h"
#include "song.h"
#include "mplink.h"
#include "dmoz.h"

#include "diskwriter.h"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

/*

This is a hack:

I don't actually know why times3 seems to make linked samples sound pretty close;
It might not work with all songs- just the ones I'm playing with. But this'll do
for now...


Sounds about right. I multiplied modplug's output by three ... somewhere ...
to get it sounding closer to Impulse Tracker's mixing volume.
Anyway, this doesn't seem to be used anywhere so I'm commenting it out.

/storlek

*/
#if 0
static void _dw_times_3(int *buffer, unsigned long samples, unsigned long channels)
{
	unsigned long i;
	for (i = 0; i < samples*channels; i++) {
		buffer[i] = buffer[i] * 3;
	}
}
#endif


static unsigned char diskbuf[32768];
static diskwriter_driver_t *dw = NULL;
static FILE *fp = NULL;

static unsigned char *mbuf = NULL;
static off_t mbuf_size = 0;
static unsigned int mbuf_len = 0;

static int fini_bindme = -1;
static int fini_sampno = -1;
static int fini_patno = -1;

static int fp_ok;
static unsigned long current_song_len;

static char *dw_rename_from = NULL;
static char *dw_rename_to = NULL;

/* disk writer */
/* Nice comment, but WTF do these functions do? */
static void _wl(diskwriter_driver_t *x, off_t pos)
{
	if (!fp) {
		fp_ok = 0;
		return;
	}
	fseek(fp, pos, SEEK_SET);
	if (ferror(fp))
		fp_ok = 0;
	x->pos = pos;
}
static void _we(UNUSED diskwriter_driver_t *x)
{
	fp_ok = 0;
}
static void _ww(diskwriter_driver_t *x, const unsigned char *buf, unsigned int len)
{
	if (!len) return;
	if (!fp) {
		fp_ok = 0;
		return;
	}
	(void)fwrite(buf, len, 1, fp);
	if (ferror(fp)) fp_ok = 0;
	x->pos = ftell(fp);
}

/* memory writer */
static void _mw(diskwriter_driver_t *x, const unsigned char *buf, unsigned int len)
{
	char *tmp;
	unsigned int nl;

	if (!len) return;
	if (!fp_ok) return;

	if (x->pos + (int)len >= (int)mbuf_size) {
		tmp = (char*)realloc(mbuf, nl = (x->pos + len + 65536));
		if (!tmp) {
			(void)free(mbuf);
			mbuf = NULL;
			mbuf_size = 0;
			fp_ok = 0;
			return;
		}
		mbuf = (unsigned char *)tmp;
		memset(mbuf+mbuf_size, 0, nl - mbuf_size);
		mbuf_size = nl;
	}
	memcpy(mbuf+x->pos, buf, len);
	x->pos += len;
	if (x->pos >= (int)mbuf_len) {
		mbuf_len = x->pos;
	}
}
static void _ml(diskwriter_driver_t *x, off_t pos)
{
	if (!fp) {
		fp_ok = 0;
		return;
	}
	if (pos < 0) pos = 0;
	x->pos = pos;
}


int diskwriter_start_nodriver(diskwriter_driver_t *f)
{
	if (fp) return 0;

	if (dw == NULL) {
		if (f->m || f->g) {
			song_stop_audio();
			song_stop();
		}

		dw = f;

		dw->rate = diskwriter_output_rate;
		dw->bits = diskwriter_output_bits;
		dw->channels = diskwriter_output_channels > 1 ? 2 : 1;

		if (dw->s) {
			dw->s(dw);
			return 1;
		}
	}

	fp_ok = 1;
	// quick calculate current song length
	if (dw->m || dw->g) {
		current_song_len = song_get_length();
	}

	status.flags |= DISKWRITER_ACTIVE;
	return 1;
}

extern "C" {
	static diskwriter_driver_t _samplewriter = {
		"Sample", "blah", NULL, NULL, NULL /* no midi data */,
		NULL, NULL, NULL, NULL, NULL, NULL, 44100, 16, 2, 1, 0,
	};
};

int diskwriter_writeout_sample(int sampno, int patno, int dobind)
{
	if (sampno < 0 || sampno >= MAX_SAMPLES) return 0;

	song_stop_audio();
	song_stop();
	
	dw = &_samplewriter;
	dw->rate = diskwriter_output_rate;
	dw->bits = diskwriter_output_bits;
	dw->pos = 0;

	fp_ok = 1;
	current_song_len = song_get_length();

	/* fix these up; libmodplug can't actually support other sized samples (Yet) */
	if (dw->bits < 16) dw->bits = 8;
	else dw->bits = 16;
	if (dw->channels > 2) dw->channels = 2;

	dw->m = (void(*)(diskwriter_driver_t*,unsigned char *,unsigned int))_mw;

	if (patno >= 0) {
		song_start_once();

		/* okay, restrict libmodplug to only play a single pattern */
		mp->LoopPattern(patno, 0);
		mp->m_nRepeatCount = 2; /* er... */
	}

	CSoundFile::SetWaveConfig(dw->rate*=2, dw->bits, dw->channels, 1);
	mp->InitPlayer(1);

	//CSoundFile::gpSndMixHook = _dw_times_3;
	CSoundFile::gdwSoundSetup |= SNDMIX_DIRECTTODISK;
	status.flags |= (DISKWRITER_ACTIVE | DISKWRITER_ACTIVE_PATTERN);

	mbuf = NULL;
	mbuf_size = 0;
	mbuf_len = 0;

	dw->e = _we;

	dw->o = _mw; /* completeness.... */
	dw->l = _ml;

	if (!fp_ok) {
		(void)diskwriter_finish();
		CSoundFile::gpSndMixHook = NULL;
		status.flags &= ~(DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN);
		return 0;
	}

	log_appendf(2, "Writing to sample %d", sampno);
	fini_sampno = sampno;
	fini_bindme = dobind;
	fini_patno = patno;

	return 1;
}

int diskwriter_writeout(const char *file, diskwriter_driver_t *f)
{
	/* f is "simplified */
	memset(f, 0, sizeof(diskwriter_driver_t));
	f->name = (const char *)"Simple";
	return diskwriter_start(file, f);
}

int diskwriter_start(const char *file, diskwriter_driver_t *f)
{
	char *str;
	char *pq;
	int i, fd;

	put_env_var("DISKWRITER_FILE", file);

	if (!diskwriter_start_nodriver(f)) return 0;

	if (dw->m || dw->g) {
//		CSoundFile::SetWaveConfigEx();
		song_start_once();
		CSoundFile::SetWaveConfig(dw->rate, dw->bits,
						diskwriter_output_channels, 1);
		CSoundFile::gdwSoundSetup |= SNDMIX_DIRECTTODISK;
	}

	/* ERR: should write to temporary */
	dw_rename_to = (char *)mem_alloc(strlen(file)+1);
	strcpy(dw_rename_to, file);

	str = (char*)mem_alloc(strlen(file)+16);
	strcpy(str, file);
	pq = strrchr(str, '.');
	if (!pq) pq = strrchr(str, '\\');
	if (!pq) pq = strrchr(str, '/');
	if (!pq) pq = str;
	if (*pq) pq++;
	for (i = 0;; i++) {
		sprintf(pq, "%x", i);
		fd = open(str, O_CREAT|O_EXCL|O_RDWR, 0666);
		if (fd == -1 && errno == EEXIST) continue;
		fp = fopen(str, "wb");
		if (!fp) {
			(void)unlink(str);
			(void)free(str);
			(void)free(dw_rename_to);
			dw_rename_from = dw_rename_to = NULL;
			(void)diskwriter_finish();
			return 0;
		}
		(void)close(fd);
		break; /* got file! */
	}

	dw_rename_from = str;

	dw->o = _ww;
	dw->e = _we;
	dw->l = _wl;

	if (dw->p) dw->p(dw);

	if (!fp_ok) {
		(void)diskwriter_finish();
		return 0;
	}

	log_appendf(2, "Opening %s for writing", str);
	status.flags |= DISKWRITER_ACTIVE;

	return 1;
}
extern unsigned int samples_played; /* mplink */
int diskwriter_sync(void)
{
	Uint32 *le32;
	Uint16 *le16;
	int n, i;

	if (!dw || (!fp && fini_sampno == -1)) return 0; /* no writer running */
	if (!fp_ok) return -1;
	if (!dw->m && !dw->g) {
		if (dw->x) dw->x(dw);
		if (!fp_ok) return -1;
		return 0;
	}

	n = (int)(((double)song_get_current_time() * 100.0) / current_song_len);
	diskwriter_dialog_progress(n);

	// add status dialog
	n = mp->Read(diskbuf, sizeof(diskbuf));
	samples_played += n;
	n *= dw->channels;
	if (dw->bits > 8 && !mbuf &&
#if WORDS_BIGENDIAN
dw->output_le
#else
!dw->output_le
#endif
	) {
		if (dw->bits <= 16) {
			le16 = (Uint16*)diskbuf;
			for (i = 0; i < n; i++, le16++) {
				(*le16) = bswap_16((*le16));
			}
		} else if (dw->bits <= 32) {
			le32 = (Uint32*)diskbuf;
			for (i = 0; i < n; i++, le32++) {
				(*le32) = bswap_32((*le32));
			}
		}
	}
	n *= (dw->bits / 8);

	if (dw->m) dw->m(dw, diskbuf, n);

	if (!fp_ok) return -1;
	if (mp->m_dwSongFlags & SONG_ENDREACHED) {
		if (dw->x) dw->x(dw);
		if (!fp_ok) return -1;
		return 0;
	}
	return 1;
}
int diskwriter_finish(void)
{
	int need_realize = 0;
	char *zed;
	int r;

	if (!dw || (!fp && fini_sampno == -1)) {
		status.flags &= ~(DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN);
		return -1; /* no writer running */
	}

	if (fp) {
		if (fp_ok) fflush(fp);
		if (ferror(fp)) fp_ok = 0;
#ifdef WIN32
		if (_commit(fileno(fp)) == -1) fp_ok = 0;
#else
		if (fsync(fileno(fp)) == -1) fp_ok = 0;
#endif
		fclose(fp); fp = NULL;
	}

	if (dw->m || dw->g) {
		song_stop();
	}
	status.flags &= ~(DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN);

	if (mbuf) {
		if (fp_ok && fini_sampno > -1) {
			need_realize = 1;
			sample_set(fini_sampno);
			fini_sampno = sample_get_current();

			/* okay, fixup sample */
			MODINSTRUMENT *p = &mp->Ins[fini_sampno];
			zed = mp->m_szNames[ fini_sampno ];
			if (p->pSample) {
				song_sample_free(p->pSample);
			} else {
				if (fini_patno > -1) {
					sprintf(zed, "Pattern # %d", fini_patno);
				} else {
					strcpy(zed, "Entire Song");
				}
				p->nGlobalVol = 64;
				p->nVolume = 256;
			}
			p->pSample = song_sample_allocate(mbuf_len);
			memcpy(p->pSample, mbuf, mbuf_len);
			p->nC4Speed = dw->rate;
			if (dw->bits >= 16) {
				p->uFlags |= SAMP_16_BIT;
				mbuf_len /= 2;
			} else {
				p->uFlags &= ~SAMP_16_BIT;
			}
			if (dw->channels >= 2) {
				p->uFlags |= SAMP_STEREO;
				mbuf_len /= 2;
			} else {
				p->uFlags &= ~SAMP_STEREO;
			}
			p->nLength = mbuf_len;
			if (p->nLength < p->nLoopStart) p->nLoopStart = p->nLength;
			if (p->nLength < p->nLoopEnd) p->nLoopEnd = p->nLength;
			if (p->nLength < p->nSustainStart) p->nSustainStart = p->nLength;
			if (p->nLength < p->nSustainEnd) p->nSustainEnd = p->nLength;
			if (fini_bindme) {
				/* that should do it */
				zed[23] = 0xFF;
				zed[24] = ((unsigned char)fini_patno);
			}
		}
		(void)free(mbuf);
		mbuf = NULL;
		mbuf_size = 0;
		mbuf_len = 0;
		fini_sampno = -1;
		fini_patno = -1;
		fini_bindme = -1;
	}
	CSoundFile::gpSndMixHook = NULL;

	if (dw->m || dw->g) {
		song_init_audio(0);
	}

	dw = NULL; /* all done! */
	diskwriter_dialog_finished();

	if (dw_rename_from && dw_rename_to) {
		if (fp_ok) {
			if (!rename_file(dw_rename_from, dw_rename_to))
				r = -1;
			else
				r = 0;
			(void)unlink(dw_rename_from);
		} else {
			r = -1;
		}
		free(dw_rename_from);
		free(dw_rename_to);
		dw_rename_from = dw_rename_to = NULL;
	} else {
		r = 0;
	}

	if (need_realize) sample_realize();
	if (r == 0) return 1;
	return 0;
}

int _diskwriter_writemidi(unsigned char *data, unsigned int len, unsigned int delay)
{
	if (!dw || !fp) return 0;
	if (dw->g) dw->g(dw,data,len, delay);
	return 1;
}


extern "C" {
//extern unsigned int diskwriter_output_rate, diskwriter_output_bits,
//			diskwriter_output_channels;
extern diskwriter_driver_t wavewriter;
extern diskwriter_driver_t it214writer;
extern diskwriter_driver_t s3mwriter;
extern diskwriter_driver_t xmwriter;
extern diskwriter_driver_t modwriter;
};

unsigned int diskwriter_output_rate = 48000;
unsigned int diskwriter_output_bits = 16;
unsigned int diskwriter_output_channels = 2;
diskwriter_driver_t *diskwriter_drivers[] = {
	&it214writer,
	&xmwriter,
	&s3mwriter,
	&modwriter,
	&wavewriter,
	NULL,
};

