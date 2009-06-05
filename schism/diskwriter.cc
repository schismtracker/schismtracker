/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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
#define NEED_BYTESWAP
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "util.h"
#include "song.h"
#include "mplink.h"
#include "dmoz.h"


#include "cmixer.h"
#include "diskwriter.h"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

/* this buffer is used for diskwriting; stdio seems to use a much
 * smaller buffer which is PAINFULLY SLOW on devices with -o sync
 */
static char dwbuf[65536];


unsigned int (*dw_multi_pCvt)(void *, int *, unsigned int, int*, int*) = clip_32_to_8;


static unsigned char diskbuf[32768];
static diskwriter_driver_t *dw = NULL;
static FILE *fp = NULL;
static FILE *multi_fp[64] = {NULL};
static int multi_mono[64] = {0};

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


static int swab_buf(int n)
{
	Uint32 *le32;
	Uint16 *le16;
	int i;

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

	return n;
}


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
	if (fwrite(buf, len, 1, fp) != 1) fp_ok = 0;
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
	if (fp)
		return DW_ERROR;

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
			return DW_OK;
		}
	}

	fp_ok = 1;
	// quick calculate current song length
	if (dw->m || dw->g) {
		current_song_len = song_get_length();
	}

	status.flags |= DISKWRITER_ACTIVE;
	return DW_OK;
}

extern "C" {
	static diskwriter_driver_t _samplewriter = {
		"Sample", "blah", 1, NULL, NULL, NULL /* no midi data */,
		NULL, NULL, NULL, NULL, NULL, NULL, 44100, 16, 2, 1, 0,
	};
};

int diskwriter_writeout_sample(int sampno, int patno, int dobind)
{
	if (sampno < 0 || sampno >= MAX_SAMPLES) return DW_ERROR;

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

	if (mp->m_dwSongFlags & SONG_NOSTEREO) dw->channels = 1;
	else if (pattern_max_channels(patno, multi_mono) == 1) dw->channels = 1;

	dw->m = (void(*)(diskwriter_driver_t*,const unsigned char *,unsigned int))_mw;

	if (patno >= 0) {
		song_start_once();

		/* okay, restrict libmodplug to only play a single pattern */
		mp->LoopPattern(patno, 0);
		mp->m_nRepeatCount = 2; /* er... */
	}

	// XXX what is this *=2 for?
	csf_set_wave_config(mp, dw->rate*=2, dw->bits, dw->channels);
        csf_init_player(mp, 1);

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
		diskwriter_finish();
		CSoundFile::gpSndMixHook = NULL;
		status.flags &= ~(DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN);
		return DW_ERROR;
	}

	log_appendf(2, "Writing to sample %d", sampno);
	fini_sampno = sampno;
	fini_bindme = dobind;
	fini_patno = patno;

	return DW_OK;
}

int diskwriter_writeout(const char *file, diskwriter_driver_t *f)
{
	/* f is "simplified */
	memset(f, 0, sizeof(diskwriter_driver_t));
	f->name = (const char *)"Simple";
	return diskwriter_start(file, f);
}

static void chan_setup(int rate, int nchan)
{
	if (dw->m || dw->g) {
		song_start_once();
                csf_set_wave_config(mp, rate, dw->bits, nchan);
		mp->gdwSoundSetup |= SNDMIX_DIRECTTODISK;
	}
}
static int chan_detect(void)
{
	int nchan = diskwriter_output_channels;
	int lim, i;
	byte *ol;

	for (i = 0; i < 64; i++) multi_mono[i] = 1;
	if (mp->m_dwSongFlags & SONG_NOSTEREO) {
		nchan = 1;
	} else {
		ol = song_get_orderlist();
		lim = 1;
		for (i = 0; i < 256; i++) {
			if (ol[i] == ORDER_SKIP) continue;
			if (ol[i] == ORDER_LAST) break;
			lim = pattern_max_channels(ol[i], multi_mono);
			if (lim > 1) break;
		}
		if (lim == 1) nchan = 1; /* mono is possible */
	}

	return dw->channels = nchan;
}

static void multi_out_helper(int chan, int *buf, int len)
{
	/* mp->_multi_out_raw = multi_out_helper; */

	int32_t vu_min[2] = { 0x7fffffff, 0x7fffffff };
	int32_t vu_max[2] = {-0x7fffffff,-0x7fffffff };

	if (!multi_fp[chan]) return;

	/* skip */
	if (dw->channels != (multi_mono[chan-1] ? 1 : 2)) return;

	if (dw->channels == 1) {
		len *= 2;
		mono_from_stereo((int *)diskbuf, len);
	}

	dw_multi_pCvt(diskbuf, buf, len, vu_min, vu_max);
	len /= (4 - (dw->bits / 8));

	len = swab_buf(len);
	if (dw->m) {
		fp = multi_fp[chan];
		dw->pos = ftell(fp);
		dw->m(dw, diskbuf, len);
	}
}

int diskwriter_multiout(const char *dir, diskwriter_driver_t *f)
{
	char *str;
	int i;

	put_env_var("DISKWRITER_DIR", dir);
	str = (char*)mem_alloc(strlen(dir)+64);
	if (!str) return DW_ERROR;
	memset(multi_fp, 0, sizeof(multi_fp));

	if (diskwriter_start_nodriver(f) != DW_OK)
		return DW_ERROR;

	dw->o = _ww;
	dw->e = _we;
	dw->l = _wl;
	fp_ok = 1;
	dw_rename_from = NULL;
	dw_rename_to = NULL;

	chan_detect();
	chan_setup(dw->rate,2);

	for (i = 1; i < 64; i++) {
		multi_fp[i] = 0;
		if (song_get_channel(i-1)->flags & CHN_MUTE) continue;

		sprintf(str, "%s%cmulti-%02d.%s",
			dir, DIR_SEPARATOR, i,
			f->extension);
		fp = multi_fp[i] = fopen(str, "wb");
		if (!multi_fp[i]) {
			fp = NULL;
			for (; i >= 1; i--) if (multi_fp[i]) fclose(multi_fp[i]);
			memset(multi_fp, 0, sizeof(multi_fp));
			diskwriter_finish();
			return DW_ERROR;
		}
		dw->channels = multi_mono[i-1] ? 1 : 2;
		if (dw->p) dw->p(dw);
	}

	if (dw->bits == 8) {
		dw_multi_pCvt = clip_32_to_8;
	} else if (dw->bits == 16) {
		dw_multi_pCvt = clip_32_to_16;
	} else if (dw->bits == 24) {
		dw_multi_pCvt = clip_32_to_24;
	} else if (dw->bits == 32) {
		dw_multi_pCvt = clip_32_to_32;
	} else {
		diskwriter_finish();
		return DW_ERROR;
	}

	dw->channels = 2;
	mp->_multi_out_raw = multi_out_helper;

	current_song_len *= 2; /* two passes */

	log_appendf(2, "Multi-out diskwriting into %s/*.%s", dir,
			dw->extension);
	status.flags |= DISKWRITER_ACTIVE;

	return DW_OK;
}
int diskwriter_start(const char *file, diskwriter_driver_t *f)
{
	char *str;
	char *pq;
	int i, fd;

	put_env_var("DISKWRITER_FILE", file);

	if (diskwriter_start_nodriver(f) != DW_OK)
		return DW_ERROR;

	chan_setup(dw->rate,chan_detect());

	/* ERR: should write to temporary */
	dw_rename_to = str_dup(file);

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
		if (fd == -1) {
			free(str);
			free(dw_rename_to);
			dw_rename_from = dw_rename_to = NULL;
			diskwriter_finish();
			return DW_ERROR;
		}
		fp = fopen(str, "wb");
		if (!fp) {
			unlink(str);
			free(str);
			free(dw_rename_to);
			dw_rename_from = dw_rename_to = NULL;
			diskwriter_finish();
			return DW_ERROR;
		}
		close(fd);
		setvbuf(fp, dwbuf, _IOFBF, sizeof(dwbuf));
		break; /* got file! */
	}

	dw_rename_from = str;

	dw->o = _ww;
	dw->e = _we;
	dw->l = _wl;

	if (dw->p)
		dw->p(dw);

	if (!fp_ok) {
		diskwriter_finish();
		return DW_ERROR;
	}

	log_appendf(2, "Opening %s for writing", str);
	status.flags |= DISKWRITER_ACTIVE;

	return DW_OK;
}

extern unsigned int samples_played; /* mplink */
int diskwriter_sync(void)
{
	unsigned int ct;
	int n;

	if (dw && mp->_multi_out_raw) {
		/* we're up */
		if (!dw) return DW_SYNC_DONE;
	} else {
		if (!dw || (!fp && fini_sampno == -1))
			return DW_SYNC_DONE; /* no writer running */
	}
	if (!fp_ok)
		return DW_SYNC_ERROR;
	if (!dw->m && !dw->g) {
		if (!dw->x) {
			/* do nothing */
		} else if (mp->_multi_out_raw) {
			for (n = 1; n < 64; n++) {
				if (!multi_fp[n]) continue;
				fp = multi_fp[n];
				dw->pos = ftell(fp);
				dw->x(dw);
			}
		} else {
			dw->x(dw);
		}
		return fp_ok ? DW_SYNC_DONE : DW_SYNC_ERROR;
	}

	ct = song_get_current_time();
	n = (int)(((double)ct * 100.0) / current_song_len);
	if (mp->_multi_out_raw && dw->channels == 1) n += 50;
	diskwriter_dialog_progress(n);

	// estimate bytes remaining
	n = (dw->rate * dw->channels * dw->bits) / 8;
	n = (current_song_len - ct) * n;
	if (n > (signed) (sizeof(diskbuf)*2)) {
		// go big
		n = sizeof(diskbuf);
	} else {
		// smallest possible fragment; near end
		n = (dw->bits * dw->channels) / 8;
	}

	n = csf_read(mp, diskbuf, sizeof(diskbuf));
	samples_played += n;

	if (mp->_multi_out_raw) {
		/* okay, it's happening elsewhere */

	} else if (dw->m) {
		n = swab_buf(n);
		dw->m(dw, diskbuf, n);
	}

	if (!fp_ok)
		return DW_SYNC_ERROR;
	if (mp->m_dwSongFlags & SONG_ENDREACHED) {
		if (mp->_multi_out_raw && dw->channels == 2) {
			/* pass 2 */
			chan_setup(dw->rate/2, 1);
			dw->channels = 1;
			return DW_SYNC_MORE;
		}


		if (!dw->x) {
			/* do nothing */
		} else if (mp->_multi_out_raw) {
			for (n = 1; n < 64; n++) {
				if (!multi_fp[n]) continue;
				fp = multi_fp[n];
				dw->pos = ftell(fp);
				dw->x(dw);
			}
		} else {
			dw->x(dw);
		}
		return fp_ok ? DW_SYNC_DONE : DW_SYNC_ERROR;
	}
	return DW_SYNC_MORE;
}

int diskwriter_finish(void)
{
	int need_realize = 0;
	char *zed;
	int r;

	if (!dw || (!fp && fini_sampno == -1)) {
		status.flags &= ~(DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN);
		return DW_NOT_RUNNING; /* no writer running */
	}

	if (mp->_multi_out_raw) {
		for (r = 1; r < 64; r++) {
			if (!multi_fp[r]) continue;
			if (fclose(multi_fp[r]) != 0) fp_ok = 0;
		}
		fp = NULL;

	} else if (fp) {
		if (fp_ok) fflush(fp);
		if (ferror(fp))
			fp_ok = 0;
#ifdef WIN32
		if (_commit(fileno(fp)) == -1)
			fp_ok = 0;
#else
		if (fsync(fileno(fp)) == -1)
			fp_ok = 0;
#endif
		fclose(fp);
		fp = NULL;
	}

	if (dw->m || dw->g)
		song_stop();

	status.flags &= ~(DISKWRITER_ACTIVE | DISKWRITER_ACTIVE_PATTERN);

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
			p->nC5Speed = dw->rate;
			p->uFlags &= ~SAMP_ADLIB;
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
		free(mbuf);
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
	mp->_multi_out_raw = NULL;

	dw = NULL; /* all done! */
	diskwriter_dialog_finished();

	if (dw_rename_from && dw_rename_to) {
		/* I SEE YOUR SCHWARTZ IS AS BIG AS MINE */
		if (status.flags & MAKE_BACKUPS)
			make_backup_file(dw_rename_to,
					status.flags & NUMBERED_BACKUPS);

		if (fp_ok) {
			r = (rename_file(dw_rename_from, dw_rename_to, 1) == DMOZ_RENAME_OK)
				? DW_OK
				: DW_ERROR;
			unlink(dw_rename_from);
		} else {
			r = DW_ERROR;
		}
		free(dw_rename_from);
		free(dw_rename_to);
		dw_rename_from = dw_rename_to = NULL;
	} else {
		r = DW_OK;
	}

	if (need_realize) sample_realize();
	return r;
}

int _diskwriter_writemidi(const unsigned char *data, unsigned int len, unsigned int delay)
{
	if (!dw || !fp)
		return DW_ERROR;
	if (dw->g)
		dw->g(dw,data,len, delay);
	return DW_OK;
}


extern "C" {
//extern unsigned int diskwriter_output_rate, diskwriter_output_bits,
//			diskwriter_output_channels;
extern diskwriter_driver_t wavewriter;
extern diskwriter_driver_t it214writer;
extern diskwriter_driver_t s3mwriter;
extern diskwriter_driver_t xmwriter;
extern diskwriter_driver_t modwriter;
extern diskwriter_driver_t mtmwriter;
extern diskwriter_driver_t midiwriter;
};

unsigned int diskwriter_output_rate = 44100;
unsigned int diskwriter_output_bits = 16;
unsigned int diskwriter_output_channels = 2;
diskwriter_driver_t *diskwriter_drivers[] = {
	&it214writer,
	&xmwriter,
	&s3mwriter,
	&modwriter,
	&mtmwriter,
	&wavewriter,
	&midiwriter,
	NULL,
};

