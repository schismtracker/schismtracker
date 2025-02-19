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
#include <stdio.h>

struct win32mm_midi {
	DWORD id;

	HMIDIOUT out;
	HMIDIIN in;

	union {
#ifdef SCHISM_WIN32_COMPILE_ANSI
		MIDIINCAPSA a;
#endif
		MIDIINCAPSW w;
	} icp;
	union {
#ifdef SCHISM_WIN32_COMPILE_ANSI
		MIDIOUTCAPSA a;
#endif
		MIDIOUTCAPSW w;
	} ocp;

	MIDIHDR hh;
	LPMIDIHDR obuf;
	unsigned char sysx[1024];
};

// whether to use ANSI or Unicode versions of functions
// (currently based on whether we're running on win9x or not)
#ifdef SCHISM_WIN32_COMPILE_ANSI
static int use_ansi_funcs = 0;
#endif

static void _win32mm_sysex(LPMIDIHDR *q, const unsigned char *d, unsigned int len)
{
	char *z;
	LPMIDIHDR m;

	if (!d) len=0;
	z = mem_calloc(1, sizeof(MIDIHDR) + len);
	m = (LPMIDIHDR)z;

	if (len) memcpy(z + sizeof(MIDIHDR), d, len);

	m->lpData = (z+sizeof(MIDIHDR));
	m->dwBufferLength = len;
	m->lpNext = *q;
	m->dwOffset = 0;
	(*q) = (m);
}

static void _win32mm_send(struct midi_port *p, const unsigned char *data,
		unsigned int len, SCHISM_UNUSED unsigned int delay)
{
	struct win32mm_midi *m;
	DWORD q;

	if (len == 0) return;

	// FIXME this shouldn't be done here!
	//if (!(p->io & MIDI_OUTPUT)) return;

	m = p->userdata;
	if (len <= 4) {
		q = data[0];
		if (len > 1) q |= (data[1] << 8);
		if (len > 2) q |= (data[2] << 16);
		if (len > 3) q |= (data[3] << 24); /* eh... */
		(void)midiOutShortMsg(m->out, q);
	} else {
		/* SysEX */
		_win32mm_sysex(&m->obuf, data, len);
		if (midiOutPrepareHeader(m->out, m->obuf, sizeof(MIDIHDR)) == MMSYSERR_NOERROR) {
			(void)midiOutLongMsg(m->out, m->obuf, sizeof(MIDIHDR));
		}
	}
}


static CALLBACK void _win32mm_inputcb(HMIDIIN in, UINT wmsg, DWORD_PTR inst,
					   DWORD_PTR param1, DWORD_PTR param2)
{
	struct midi_port *p = (struct midi_port *)inst;
	struct win32mm_midi *m;
	unsigned char c[4];

	switch (wmsg) {
	case MIM_OPEN:
		timer_msleep(0); /* eh? */
	case MIM_CLOSE:
		break;
	case MIM_DATA:
		c[0] = param1 & 255;
		c[1] = (param1 >> 8) & 255;
		c[2] = (param1 >> 16) & 255;
		midi_received_cb(p, c, 3);
		break;
	case MIM_LONGDATA:
		{
			MIDIHDR* hdr = (MIDIHDR*) param1;
			if (hdr->dwBytesRecorded > 0)
			{
				/* long data */
				m = p->userdata;
				midi_received_cb(p, (unsigned char *) m->hh.lpData, m->hh.dwBytesRecorded);
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
	WORD r;

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
	if (p->io & MIDI_OUTPUT) {
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
	if (p->io & MIDI_INPUT) {
		/* portmidi appears to (essentially) ignore the error codes
		for these guys */
		(void)midiInStop(m->in);
		(void)midiInReset(m->in);
		(void)midiInUnprepareHeader(m->in,&m->hh,sizeof(m->hh));
		(void)midiInClose(m->in);
	}
	if (p->io & MIDI_OUTPUT) {
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
	static unsigned int last_known_in_port = 0;
	static unsigned int last_known_out_port = 0;

	struct win32mm_midi *data;

	UINT i;
	UINT mmin, mmout;
	WORD r;

	mmin = midiInGetNumDevs();
	for (i = last_known_in_port; i < mmin; i++) {
		data = mem_calloc(1, sizeof(struct win32mm_midi));
#ifdef SCHISM_WIN32_COMPILE_ANSI
		if (use_ansi_funcs) {
			r = midiInGetDevCapsA(i, &data->icp.a, sizeof(data->icp.a));
		} else
#endif
		{
			r = midiInGetDevCapsW(i, &data->icp.w, sizeof(data->icp.w));
		}
		if (r != MMSYSERR_NOERROR) {
			free(data);
			continue;
		}
		data->id = i;

		char *utf8;
#ifdef SCHISM_WIN32_COMPILE_ANSI
		if (use_ansi_funcs) {
			if (charset_iconv(data->ocp.a.szPname, &utf8, CHARSET_ANSI, CHARSET_UTF8, sizeof(data->ocp.a.szPname)))
				continue;
		} else
#endif
		{
			if (charset_iconv(data->ocp.w.szPname, &utf8, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(data->ocp.w.szPname)))
				continue;
		}

		midi_port_register(p, MIDI_INPUT, utf8, data, 1);
	}
	last_known_in_port = mmin;

	mmout = midiOutGetNumDevs();
	for (i = last_known_out_port; i < mmout; i++) {
		data = mem_calloc(1, sizeof(struct win32mm_midi));
#ifdef SCHISM_WIN32_COMPILE_ANSI
		if (use_ansi_funcs) {
			r = midiOutGetDevCapsA(i, &data->ocp.a, sizeof(data->ocp.a));
		} else
#endif
		{
			r = midiOutGetDevCapsW(i, &data->ocp.w, sizeof(data->ocp.w));
		}
		if (r != MMSYSERR_NOERROR) {
			if (data) free(data);
			continue;
		}
		data->id = i;

		char *utf8;
#ifdef SCHISM_WIN32_COMPILE_ANSI
		if (use_ansi_funcs) {
			if (charset_iconv(data->ocp.a.szPname, &utf8, CHARSET_ANSI, CHARSET_UTF8, sizeof(data->ocp.a.szPname)))
				continue;
		} else
#endif
		{
			if (charset_iconv(data->ocp.w.szPname, &utf8, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(data->ocp.w.szPname)))
				continue;
		}
		
		midi_port_register(p, MIDI_OUTPUT, utf8, data, 1);
	}
	last_known_out_port = mmout;
}

int win32mm_midi_setup(void)
{
	static struct midi_driver driver = {0};
	TIMECAPS caps;

	driver.flags = 0;
	driver.poll = _win32mm_poll;
	driver.thread = NULL;
	driver.enable = _win32mm_start;
	driver.disable = _win32mm_stop;
	driver.send = _win32mm_send;

#ifdef SCHISM_WIN32_COMPILE_ANSI
	use_ansi_funcs = (GetVersion() & UINT32_C(0x80000000));
#endif

	if (!midi_provider_register("Win32MM", &driver)) return 0;

	return 1;
}
