/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
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

#include "log.h"
#include "midi.h"
#include "timer.h"
#include "loadso.h"
#include "charset.h"
#include "mem.h"
#include "osdefs.h"

#include "util.h"

#include <windows.h>
#include <mmsystem.h>

union win32mm_icp {
#ifdef SCHISM_WIN32_COMPILE_ANSI
	MIDIINCAPSA a;
#endif
	MIDIINCAPSW w;
};

union win32mm_ocp {
#ifdef SCHISM_WIN32_COMPILE_ANSI
	MIDIOUTCAPSA a;
#endif
	MIDIOUTCAPSW w;
};

struct win32mm_midi {
	DWORD id;

	HMIDIIN in;
	HMIDIOUT out;

	union win32mm_icp icp;
	union win32mm_ocp ocp;

	MIDIHDR hh;
	LPMIDIHDR obuf;
	unsigned char sysx[1024];
};

static void _win32mm_sysex(LPMIDIHDR *q, const unsigned char *data, uint32_t len)
{
	struct {
		MIDIHDR hdr;
		char data[SCHISM_FAM_SIZE];
	} *m;

	if (!data) len = 0;

	m = mem_calloc(1, sizeof(*m) + len);

	if (len) memcpy(m->data, data, len);

	m->hdr.lpData = m->data;
	m->hdr.dwBufferLength = len;
	m->hdr.lpNext = *q;
	m->hdr.dwOffset = 0;

	*q = &m->hdr;
}

static void _win32mm_send(struct midi_port *p, const unsigned char *data,
		uint32_t len, SCHISM_UNUSED uint32_t delay)
{
	struct win32mm_midi *m;

	if (len == 0) return;

	m = p->userdata;
	if (len <= 4) {
		DWORD q;

		q = data[0];
		if (len > 1) q |= (data[1] << 8);
		if (len > 2) q |= (data[2] << 16);
		if (len > 3) q |= (data[3] << 24); /* eh... */
		(void)midiOutShortMsg(m->out, q);
	} else {
		/* SysEX */
		_win32mm_sysex(&m->obuf, data, len);
		if (midiOutPrepareHeader(m->out, m->obuf, sizeof(MIDIHDR)) == MMSYSERR_NOERROR)
			(void)midiOutLongMsg(m->out, m->obuf, sizeof(MIDIHDR));
	}
}


static CALLBACK void _win32mm_inputcb(HMIDIIN in, UINT wmsg, DWORD_PTR inst,
	DWORD_PTR param1, SCHISM_UNUSED DWORD_PTR param2)
{
	struct midi_port *p = (struct midi_port *)inst;

	switch (wmsg) {
	case MIM_OPEN:
		timer_msleep(0); /* eh? */
	case MIM_CLOSE:
		break;
	case MIM_DATA: {
		unsigned char c[4];

		c[0] = param1 & 0xFF;
		c[1] = (param1 >> 8) & 0xFF;
		c[2] = (param1 >> 16) & 0xFF;
		midi_received_cb(p, c, 3);
		break;
	}
	case MIM_LONGDATA: {
		MIDIHDR *hdr = (MIDIHDR *)param1;
		if (hdr->dwBytesRecorded > 0) {
			/* long data */
			struct win32mm_midi *m = p->userdata;

			midi_received_cb(p, (unsigned char *)m->hh.lpData, m->hh.dwBytesRecorded);
			// I don't see any reason we can't do this here (midi_received_cb does not
			// retain the pointer to the data after it returns)
			midiInAddBuffer(in, &m->hh, sizeof(m->hh));
		}
		break;
	}
	}
}


static int _win32mm_start(struct midi_port *p)
{
	struct win32mm_midi *m;
	UINT id;
	MMRESULT r;

	m = p->userdata;
	id = m->id;
	if (p->io == MIDI_INPUT) {
		m->in = NULL;
		r = midiInOpen(&m->in,
				(UINT_PTR)id,
				(DWORD_PTR)_win32mm_inputcb,
				(DWORD_PTR)p,
				CALLBACK_FUNCTION);
		if (r != MMSYSERR_NOERROR) return 0;
		memset(&m->hh, 0, sizeof(m->hh));
		m->hh.lpData = (LPSTR)m->sysx;
		m->hh.dwBufferLength = sizeof(m->sysx);
		m->hh.dwFlags = 0;
		r = midiInPrepareHeader(m->in, &m->hh, sizeof(MIDIHDR));
		if (r != MMSYSERR_NOERROR) return 0;
		r = midiInAddBuffer(m->in, &m->hh, sizeof(MIDIHDR));
		if (r != MMSYSERR_NOERROR) return 0;
		if (midiInStart(m->in) != MMSYSERR_NOERROR) return 0;
	}
	if (p->io == MIDI_OUTPUT) {
		m->out = NULL;
		if (midiOutOpen(&m->out,
				(UINT_PTR)id,
				0, 0,
				CALLBACK_NULL) != MMSYSERR_NOERROR) return 0;
	}

	return 1;
}
static int _win32mm_stop(struct midi_port *p)
{
	struct win32mm_midi *m;
	LPMIDIHDR ptr;

	m = p->userdata;
	if (p->io == MIDI_INPUT) {
		/* portmidi appears to (essentially) ignore the error codes
		for these guys */
		(void)midiInStop(m->in);
		(void)midiInReset(m->in);
		(void)midiInUnprepareHeader(m->in,&m->hh,sizeof(m->hh));
		(void)midiInClose(m->in);
	}
	if (p->io == MIDI_OUTPUT) {
		(void)midiOutReset(m->out);
		(void)midiOutClose(m->out);
		/* free output chain */
		ptr = m->obuf;
		while (ptr) {
			m->obuf = m->obuf->lpNext;
			(void)free(m->obuf);
			ptr = m->obuf;
		}
	}
	return 1;
}


static void _win32mm_poll(struct midi_provider *p)
{
	struct midi_port *q;
	struct win32mm_midi *data;
	UINT i, mmin, mmout;
	MMRESULT r;

	/* mark all ports for removal */
	midi_provider_mark_ports(p);

	mmin = midiInGetNumDevs();
	mmout = midiOutGetNumDevs();

#define SCANPORTS(COUNT, GETCAPS, IOCAP, CP) \
	do { \
		for (i = 0; i < (COUNT); i++) { \
			union win32mm_##CP cp; \
			int ok; \
			char *utf8; \
		\
			SCHISM_ANSI_UNICODE({ \
				r = GETCAPS##A(i, &cp.a, sizeof(cp.a)); \
			}, { \
				r = GETCAPS##W(i, &cp.w, sizeof(cp.w)); \
			}) \
		\
			if (r != MMSYSERR_NOERROR) \
				continue; \
		\
			/* does this port already exist? */ \
			q = NULL; \
			ok = 0; \
			while (midi_port_foreach(p, &q)) { \
				if (!q->mark || !(q->iocap == IOCAP)) \
					continue; \
		\
				data = q->userdata; \
				if (!memcmp(&data->CP, &cp, sizeof(cp))) { \
					q->mark = 0; \
					/* fixup the ID, if any */ \
					data->id = i; \
					ok = 1; \
					break; \
				} \
			} \
		\
			if (ok) \
				continue; \
		\
			data = mem_calloc(1, sizeof(*data)); \
			data->id = i; \
			memcpy(&data->CP, &cp, sizeof(cp)); \
		\
			SCHISM_ANSI_UNICODE({ \
				if (charset_iconv(data->CP.a.szPname, &utf8, CHARSET_ANSI, CHARSET_UTF8, sizeof(data->CP.a.szPname))) \
					continue; \
			}, { \
				if (charset_iconv(data->CP.w.szPname, &utf8, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(data->CP.w.szPname))) \
					continue; \
			}) \
		\
			midi_port_register(p, IOCAP, utf8, data, 1); \
		\
			free(utf8); \
		} \
	} while (0)

	/* ~everybody wants prosthetic foreheads on their real heads~ ! */
	SCANPORTS(mmin,  midiInGetDevCaps,  MIDI_INPUT,  icp);
	SCANPORTS(mmout, midiOutGetDevCaps, MIDI_OUTPUT, ocp);

#undef SCANPORTS

	midi_provider_remove_marked_ports(p);
}

int win32mm_midi_setup(void)
{
	static const struct midi_driver driver = {
		.flags = 0,
		.poll = _win32mm_poll,
		.thread = NULL,
		.enable = _win32mm_start,
		.disable = _win32mm_stop,
		.send = _win32mm_send,
	};

	if (!midi_provider_register("Win32MM", &driver)) return 0;

	return 1;
}
