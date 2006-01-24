/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#include "midi.h"

#include "mixer.h"
#include "util.h"

#ifdef USE_WIN32MM

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

struct win32mm_midi {
	DWORD id;

	HMIDIOUT out;
	HMIDIIN in;

	MIDIINCAPS icp;
	MIDIOUTCAPS ocp;

	MIDIHDR hh;
	LPMIDIHDR obuf;
	unsigned char sysx[1024];
};
static unsigned int last_known_in_port = 0;
static unsigned int last_known_out_port = 0;

static MMRESULT (*XP_timeSetEvent)(UINT u,UINT r,LPTIMECALLBACK proc,
			DWORD_PTR user, UINT flags) = 0;

static void _win32mm_sysex(LPMIDIHDR *q, unsigned char *d, unsigned int len)
{
	unsigned char *z;
	LPMIDIHDR m;

	z = mem_alloc(sizeof(MIDIHDR) + len + 8);
	m = (LPMIDIHDR)z;
	
	memset(m, 0, sizeof(MIDIHDR));
	if (d) memcpy(z + sizeof(MIDIHDR), d, len);

	m->lpData = (z+sizeof(MIDIHDR));
	m->dwBufferLength = len+8;
	m->dwBytesRecorded = (d ? len : 0);
	m->lpNext = *q;
	m->dwOffset = 0;
	(*q) = (m);
}

static CALLBACK void _win32mm_xp_output(UNUSED UINT uTimerID,
			UNUSED UINT uMsg,
			DWORD_PTR dwUser,
			DWORD_PTR dw1,
			DWORD_PTR dw2)
{
	LPMIDIHDR x;
	HMIDIOUT out;

	x = (LPMIDIHDR)dwUser;
	out = (HMIDIOUT)x->dwUser;

	if (midiOutPrepareHeader(out, x, sizeof(MIDIHDR)) == MMSYSERR_NOERROR) {
		(void)midiOutLongMsg(out, x, sizeof(MIDIHDR));
	}
}
static void _win32mm_send_xp(struct midi_port *p, unsigned char *data,
		unsigned int len, unsigned int delay)
{
	/* version for windows XP */
	struct win32mm_midi *m;

	if (len == 0) return;
	m = p->userdata;

	_win32mm_sysex(&m->obuf, data, len);
	m->obuf->dwUser = (DWORD_PTR)m->out;
	if (XP_timeSetEvent(delay, 0, _win32mm_xp_output, (DWORD_PTR)m->obuf,
			TIME_ONESHOT | TIME_CALLBACK_FUNCTION) == NULL) {
		/* slow... */
		if (midiOutPrepareHeader(m->out, m->obuf, sizeof(MIDIHDR)) == MMSYSERR_NOERROR) {
			(void)midiOutLongMsg(m->out, m->obuf, sizeof(MIDIHDR));
		}
		return;
	}
	/* will happen... */
}

static void _win32mm_send(struct midi_port *p, unsigned char *data,
		unsigned int len, UNUSED unsigned int delay)
{
	struct win32mm_midi *m;
	DWORD q;

	if (len == 0) return;

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


static CALLBACK void  _win32mm_inputcb(HMIDIIN in, UINT wmsg, DWORD inst,
						DWORD param1, DWORD param2)
{
	struct midi_port *p = (struct midi_port *)inst;
	struct win32mm_midi *m;
	unsigned char c[4];

	switch (wmsg) {
	case MIM_OPEN:
		SDL_Delay(0); /* eh? */
	case MIM_CLOSE:
		break;
	case MIM_DATA:
		c[0] = param1 & 255;
		c[1] = (param1 >> 8) & 255;
		c[2] = (param1 >> 16) & 255;
		midi_received_cb(p, c, 3);
		break;
	case MIM_LONGDATA:
		/* long data */
		m = p->userdata;
		midi_received_cb(p, m->hh.lpData, m->hh.dwBytesRecorded);
		m->hh.dwBytesRecorded = 0;
		(void)midiInAddBuffer(m->in, &m->hh, sizeof(MIDIHDR));
		break;
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
	WORD r;

	m = p->userdata;
	if (p->io & MIDI_INPUT) {
		/* portmidi appears to (essentially) ignore the error codes
		for these guys */
		(void)midiInStop(m->in);
		(void)midiInReset(m->in);
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
	struct midi_port *ptr;
	struct win32mm_midi *data;

	UINT i;
	UINT mmin, mmout;
	WORD r;

	mmin = midiInGetNumDevs();
	for (i = last_known_in_port; i < mmin; i++) {
		data = mem_alloc(sizeof(struct win32mm_midi));
		memset(data,0,sizeof(struct win32mm_midi));
		r = midiInGetDevCaps(i, (LPMIDIINCAPS)&data->icp,
					sizeof(MIDIINCAPS));
		if (r != MMSYSERR_NOERROR) {
			free(data);
			continue;
		}
		data->id = i;
		midi_port_register(p, MIDI_INPUT, data->icp.szPname, data, 1);
	}
	last_known_in_port = mmin;

	mmout = midiOutGetNumDevs();
	for (i = last_known_out_port; i < mmout; i++) {
		data = mem_alloc(sizeof(struct win32mm_midi));
		memset(data,0,sizeof(struct win32mm_midi));
		r = midiOutGetDevCaps(i, (LPMIDIOUTCAPS)&data->ocp,
					sizeof(MIDIOUTCAPS));
		if (r != MMSYSERR_NOERROR) {
			if (data) free(data);
			continue;
		}
		data->id = i;
		midi_port_register(p, MIDI_OUTPUT, data->ocp.szPname, data, 1);
	}
	last_known_out_port = mmout;
}

int win32mm_midi_setup(void)
{
	struct midi_driver driver;
        HINSTANCE winmm;

	driver.flags = 0;
	driver.poll = _win32mm_poll;
	driver.thread = NULL;
	driver.enable = _win32mm_start;
	driver.disable = _win32mm_stop;

        winmm = GetModuleHandle("WINMM.DLL");
        if (!winmm) winmm = LoadLibrary("WINMM.DLL");
        if (!winmm) winmm = GetModuleHandle("WINMM.DLL");
	if (winmm) {
		XP_timeSetEvent = (void*)GetProcAddress(winmm,"timeSetEvent");
		if (XP_timeSetEvent) {
			driver.send = _win32mm_send_xp;
			driver.flags |= MIDI_PORT_CAN_SCHEDULE;
		} else {
			driver.send = _win32mm_send;
		}
	} else {
		XP_timeSetEvent = 0;
		driver.send = _win32mm_send;
	}

	if (!midi_provider_register("Win32MM", &driver)) return 0;

	return 1;
}


#endif
