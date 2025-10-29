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

/* Support for MIDI through Mac OS's MIDI Manager.
 * Maybe at some point we can use OMS, once I've figured out everything.
 * Though the API seems a bit eerily similar to this!
 *
 * TODO: MIDI Manager actually DOES have support for scheduled MIDI
 * sending, but it requires a bit more care than what I've hacked up.
 *
 * Also this code is pretty much entirely untested for now...
 * I need to figure out how to get MIDI Manager and OMS to talk
 * to each other so I can properly get MIDI In/Out from one of
 * my keyboards. :') */

#include "headers.h"
#include "mem.h"
#include "str.h"
#include "log.h"

#include "midi.h" /* schism midi header */

#include <MIDI.h>
#include <Memory.h>

#define SCHISM_MIDIMGR_CLIENT_ID (0x5363686D)      /* Schm */
#define SCHISM_MIDIMGR_INPUT_PORT_ID (0x494E5055)  /* INPU */
#define SCHISM_MIDIMGR_OUTPUT_PORT_ID (0x4F555450) /* OUTP */
#define SCHISM_MIDIMGR_CLIENT_NAME "Schism Tracker"
#define SCHISM_MIDIMGR_INPUT_PORT_NAME "MIDI In"
#define SCHISM_MIDIMGR_OUTPUT_PORT_NAME "MIDI Out"
#define SCHISM_MIDIMGR_PORT_BUFFER_SIZE (1024)     /* ehhhh */

/* Enable debug prints.
 * This should probably be enabled always considering how
 * relatily untested everything still is.
 *
 * TODO maybe we could have a toggle-option in the log to show
 * "debug" info? Our log still kinds of sucks lol */
#define SCHISM_MIDIMGR_DEBUG 1

#ifdef SCHISM_MIDIMGR_DEBUG
static void midimgr_logf(const char *fmt, ...)
{
	va_list ap;
	char *s;
	int r;

	va_start(ap, fmt);
	r = vasprintf(&s, fmt, ap);
	va_end(ap);

	if (r < 0)
		return;

	log_appendf(1, "[MIDImgr]: %s", s);

	free(s);
}
#endif

struct midimgr_midi_provider {
	short refnum_in;
	short refnum_out;
	UniversalProcPtr readhook_proc;
};

struct midimgr_midi {
	/* client ID & port ID; these are unique to
	 * the MIDI device so we can use them for
	 * polling purposes */
	uint32_t clientID;
	uint32_t portID;
};

static void _midimgr_send(struct midi_port *p, const unsigned char *data,
	uint32_t len, SCHISM_UNUSED uint32_t delay)
{
	/* FIXME: If we have multiple ports, this will send data to ALL
	 * of them. That's really bad! :)
	 *
	 * NOTE: this problem also exists with JACK midi as well, OOPS!*/
	struct midimgr_midi_provider *q = p->provider->userdata;
	MIDIPacket pkt;

	/*
	MIDI data and messages are passed in MIDIPacket records (see below).
	The first byte of every MIDIPacket contains a set of flags

	bits 0-1 00 = new MIDIPacket, not continued
	01 = begining of continued MIDIPacket
	10 = end of continued MIDIPacket
	11 = continuation
	bits 2-3 reserved

	bits 4-6 000 = packet contains MIDI data

	001 = packet contains MIDI Manager message

	bit 7 0 = MIDIPacket has valid stamp
	1 = stamp with current clock
	*/

	/* MIDI Manager has a fixed maximum packet length of 248.
	 * So if we need to pass any data that's over that limit
	 * we must separate it across multiple MIDIWritePacket calls. */
	if (len > 248) {
		uint32_t offset;

		pkt.flags = 0x81; /* Beginning of continued packet */
		pkt.len = 248; /* Fixed packet length */

		offset = 0;

		while (len > 248) {
			memcpy(pkt.data, data + offset, 248);
			MIDIWritePacket(q->refnum_out, &pkt);

			pkt.flags = 0x83; /* Continuation */

			len -= 248;
			offset += 248;
		}

		/* Now, we have at least one byte left that we can send
		 * to end the packet. */

		pkt.flags = 0x82; /* End of continued packet */
		pkt.len = len;
		memcpy(pkt.data, data + offset, len);

		MIDIWritePacket(q->refnum_out, &pkt);
	} else {
		pkt.flags = 0x80;
		pkt.len = len;
		memcpy(pkt.data, data, len);

		MIDIWritePacket(q->refnum_out, &pkt);
	}
}

static int _midimgr_start(struct midi_port *p)
{
	struct midimgr_midi *mgr = p->userdata;
	OSErr err;

	if (p->iocap == MIDI_INPUT) {
		/* Connect their output to our input */
		err = MIDIConnectData(mgr->clientID, mgr->portID,
			SCHISM_MIDIMGR_CLIENT_ID, SCHISM_MIDIMGR_INPUT_PORT_ID);
	} else if (p->iocap == MIDI_OUTPUT) {
		/* Connect our output to their input */
		err = MIDIConnectData(SCHISM_MIDIMGR_CLIENT_ID,
			SCHISM_MIDIMGR_OUTPUT_PORT_ID, mgr->clientID, mgr->portID);
	}

	return (err == noErr);
}

static int _midimgr_stop(struct midi_port *p)
{
	/* "UnConnect" ? really? */
	struct midimgr_midi *mgr = p->userdata;
	OSErr err;

	if (p->iocap == MIDI_INPUT) {
		/* Disconnect their output from our input */
		err = MIDIUnConnectData(mgr->clientID, mgr->portID,
			SCHISM_MIDIMGR_CLIENT_ID, SCHISM_MIDIMGR_INPUT_PORT_ID);
	} else if (p->iocap == MIDI_OUTPUT) {
		/* Disconnect our output from their input */
		err = MIDIUnConnectData(SCHISM_MIDIMGR_CLIENT_ID,
			SCHISM_MIDIMGR_OUTPUT_PORT_ID, mgr->clientID, mgr->portID);
	}

	return (err == noErr);
}

static void _midimgr_destroy_port_userdata(void *userdata)
{
	/* okay! */
	free(userdata);
}

/* separated from _midimgr_poll */
static void _midimgr_handle_port(struct midi_provider *q,
	uint32_t clientID, uint32_t portID)
{
	struct midi_port *p;
	uint8_t iocap;
	unsigned char name[257]; /* okay */
	struct midimgr_midi *mgr;

	{
		/* Need to grab MIDI port info for the port type */
		MIDIPortInfoHdl info;
		int16_t type;

		info = MIDIGetPortInfo(clientID, portID);
		if (!info)
			return;

		HLock((Handle)info);
		type = (*info)->portType;
		HUnlock((Handle)info);

		/* No longer need the handle */
		DisposeHandle((Handle)info);

		/* Convert port type to iocaps */
		switch (type) {
		case 2: /* This port outputs data. To us, this is an input */
			iocap = MIDI_INPUT;
			break;
		case 1: /* This port takes in data (output) */
			iocap = MIDI_OUTPUT;
			break;
		default: /* Time port, or something weird. I don't care */
			return;
		}
	}

	/* first, search for the port in the list. if it exists, all
	 * we need to do is unmark it and return */
	p = NULL;
	while (midi_port_foreach(q, &p)) {
		if (!p->mark || (p->iocap != iocap))
			continue; /* alright */

		mgr = p->userdata;
		if (mgr->clientID == clientID && mgr->portID == portID) {
			/* unmark the port, and return without doing anything */
			p->mark = 0;
			return;
		}
	}

	/* otherwise, we have to add it to the list.
	 * this means getting the name, too */
	name[0] = 0;
	MIDIGetPortName(clientID, portID, name);

	/* Convert the pascal string into a C-string in-place.
	 * We (theoretically) won't overflow here, since the size of
	 * the array is one more than what we actually need to store
	 * JUST the Pascal string. */
	(name + 1)[name[0]] = 0;
	/* (name + 1) is the name as a C-string */

	mgr = mem_calloc(1, sizeof(struct midimgr_midi));

	mgr->clientID = clientID;
	mgr->portID   = portID;

	midi_port_register(q, iocap, name + 1, mgr,
		_midimgr_destroy_port_userdata);
}

static void _midimgr_poll(struct midi_provider *q)
{
	/* XXX should we be calling DisposeHandle on these? or is the memory
	 * shared for all users? idk */
	MIDIIDListHdl clientlisthdl;
	uint16_t i;

	/* Get all clients */
	clientlisthdl = MIDIGetClients();
	if (!clientlisthdl) {
#ifdef SCHISM_MIDIMGR_DEBUG
		midimgr_logf("Failed to receive MIDI clients");
#endif
		return; /* What? */
	}

	/* do this before we lock */
	midi_provider_mark_ports(q);

	HLock((Handle)clientlisthdl);
	/* wish we had a defer mechanism */

	for (i = 0; i < (*clientlisthdl)->numIDs; i++) {
		MIDIIDListHdl portlisthdl;
		uint16_t j;
		uint32_t clientID = (*clientlisthdl)->list[i];

		/* Ignore our own ports if this ever happens */
		if (clientID == SCHISM_MIDIMGR_CLIENT_ID)
			continue;

		portlisthdl = MIDIGetPorts(clientID);
		if (!portlisthdl) {
#ifdef SCHISM_MIDIMGR_DEBUG
			midimgr_logf("Failed to receive MIDI ports for client 0x%" PRIx32, clientID);
#endif
			continue; /* Client was removed in between the call? */
		}

		HLock((Handle)portlisthdl);

		for (j = 0; j < (*portlisthdl)->numIDs; j++) {
			/* Incoming port handling code separated out to
			 * make the logic simpler */
			_midimgr_handle_port(q, clientID, (*portlisthdl)->list[j]);
		}

		HUnlock((Handle)portlisthdl);
		DisposeHandle((Handle)portlisthdl);
	}

	HUnlock((Handle)clientlisthdl);
	DisposeHandle((Handle)clientlisthdl);

	midi_provider_remove_marked_ports(q);
}

/* { Valid results to be returned by readHooks }
midiKeepPacket = 0;
midiMorePacket = 1;
midiNoMorePacket = 2; */
static pascal short _midimgr_read_hook(MIDIPacketPtr pkt, uint32_t refCon)
{
	/* Just a stub for now to make sure I've handled all of the
	 * universal procedure pointer things correctly. */
#ifdef SCHISM_MIDIMGR_DEBUG
	midimgr_logf("Received MIDI packet of length %u\n", pkt->len);
#endif
	return 2;
}

static void _midimgr_free_provider(void *userdata)
{
	struct midimgr_midi_provider *q = userdata;

	/* Kill off the ports */
	MIDIRemovePort(q->refnum_in);
	MIDIRemovePort(q->refnum_out);
	DisposeMIDIReadHookUPP(q->readhook_proc);

	free(q);

	/* Finally, sign out */
	MIDISignOut(SCHISM_MIDIMGR_CLIENT_ID);
}

int midimgr_midi_setup(void)
{
	/* FIXME do proper error handling everywhere in this function */
	static const struct midi_driver driver = {
		.flags = 0,
		.poll = _midimgr_poll,
		.thread = NULL,
		.work = NULL,
		.enable = _midimgr_start,
		.disable = _midimgr_stop,
		.send = _midimgr_send,
	};
	OSErr err;
	unsigned char pstr[256];
	struct midimgr_midi_provider *prov;
	MIDIPortParams params;
	struct midi_provider *q;

	prov = mem_calloc(1, sizeof(*prov));

	/* "refCon" is i think just macos-speak for the typical C
	 * "userdata"/"opaque" pointer */
	str_to_pascal(SCHISM_MIDIMGR_CLIENT_NAME, pstr, NULL);
	err = MIDISignIn(SCHISM_MIDIMGR_CLIENT_ID, 0, NULL, pstr);

	if (err != noErr) {
#ifdef SCHISM_MIDIMGR_DEBUG
		midimgr_logf("Failed to sign in with error %d", (int)err);
#endif
		free(prov);
		return 0;
	}

	/* The MIDI Manager is 68k code, so we have to wrap our
	 * callback in a routine descriptor which handles everything
	 * transparently. */
	prov->readhook_proc = NewMIDIReadHookUPP(_midimgr_read_hook);

	/* Set up params for input port */
	params.portID = SCHISM_MIDIMGR_INPUT_PORT_ID;
	params.portType = 1; /* Input */
	params.timeBase = 0; /* Time base (we have none) */
	params.offsetTime = 0; /* Offset for current time stamps ??? */
	params.readHook = prov->readhook_proc;
	params.refCon = (long)prov;
	params.initClock.syncType = midiInternalSync; /* Internal sync */
	params.initClock.curTime = 0;
	params.initClock.format = midiFormatMSec; /* Milliseconds */
	str_to_pascal(SCHISM_MIDIMGR_INPUT_PORT_NAME, params.name, NULL);

	err = MIDIAddPort(SCHISM_MIDIMGR_CLIENT_ID, SCHISM_MIDIMGR_PORT_BUFFER_SIZE,
		&prov->refnum_in, &params);
	if (err != noErr) {
#ifdef SCHISM_MIDIMGR_DEBUG
		midimgr_logf("Failed to add input port with error %d", (int)err);
#endif
		free(prov);
		return 0;
	}

	/* Set up params for output port */
	params.portID = SCHISM_MIDIMGR_OUTPUT_PORT_ID;
	params.portType = 2; /* Output */
	params.timeBase = 0; /* Time base (we have none) */
	params.offsetTime = 0; /* Offset for current time stamps ??? */
	params.readHook = NULL; /* None */
	params.refCon = (long)prov;
	params.initClock.syncType = midiInternalSync; /* Internal sync */
	params.initClock.curTime = 0;
	params.initClock.format = midiFormatMSec; /* Milliseconds */
	str_to_pascal(SCHISM_MIDIMGR_OUTPUT_PORT_NAME, params.name, NULL);

	err = MIDIAddPort(SCHISM_MIDIMGR_CLIENT_ID, SCHISM_MIDIMGR_PORT_BUFFER_SIZE,
		&prov->refnum_out, &params);
	if (err != noErr) {
#ifdef SCHISM_MIDIMGR_DEBUG
		midimgr_logf("Failed to add output port with error %d", (int)err);
#endif
		free(prov);
		return 0;
	}

	q = midi_provider_register("Macintosh MIDI Manager", &driver, prov);
	if (!q) {
		free(prov);
		return 0;
	}

	return 1;
}
