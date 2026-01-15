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

#include "midi.h"
#include "mem.h"
#include "util.h"

#include <CoreServices/CoreServices.h>
#include <CoreMIDI/MIDIServices.h>
#include <CoreAudio/HostTime.h>

/* TODO move these into a structure! */
struct macosx_midi_provider {
	MIDIClientRef client; // = 0;
	MIDIPortRef portIn;   // = 0;
	MIDIPortRef portOut;  // = 0;
};

struct macosx_midi {
	MIDIEndpointRef ep;
	unsigned char packet[1024];
	MIDIPacketList *pl;
	MIDIPacket *x;
};

static void readProc(const MIDIPacketList *np, SCHISM_UNUSED void *rc, void *crc)
{
	struct midi_port *p;
	struct macosx_midi *m;
	MIDIPacket *x;
	uint32_t i;
	struct macosx_midi_provider *pp;

	p = crc;
	m = p->userdata;
	pp = rc;

	x = (MIDIPacket*)&np->packet[0];
	for (i = 0; i < np->numPackets; i++) {
		midi_received_cb(p, x->data, x->length);
		x = MIDIPacketNext(x);
	}
}
static void macosx_send_(struct midi_port *p, const unsigned char *data, uint32_t len,
	uint32_t delay)
{
	struct macosx_midi *m = (struct macosx_midi *)p->userdata;

	if (!m->x)
		m->x = MIDIPacketListInit(m->pl);

	/* msec to nsec? */
	m->x = MIDIPacketListAdd(m->pl, sizeof(m->packet),
			m->x, (MIDITimeStamp)AudioConvertNanosToHostTime(
			AudioConvertHostTimeToNanos(AudioGetCurrentHostTime()) + (1000000 * delay)),
			len, data);
}
static void macosx_drain_(struct midi_port *p)
{
	struct macosx_midi *m;
	struct macosx_midi_provider *mp;

	m = (struct macosx_midi *)p->userdata;
	mp = p->provider->userdata;
	if (m->x) {
		MIDISend(mp->portOut, m->ep, m->pl);
		m->x = NULL;
	}
}

/* lifted from portmidi */
static void get_ep_name(MIDIEndpointRef ep, char *buf, size_t buf_len)
{
	MIDIEntityRef entity;
	MIDIDeviceRef device;
	CFStringRef endpointName = NULL, deviceName = NULL, fullName = NULL;

	/* get the entity and device info */
	MIDIEndpointGetEntity(ep, &entity);
	MIDIEntityGetDevice(entity, &device);

	/* create the nicely formatted name */
	MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &endpointName);
	MIDIObjectGetStringProperty(device, kMIDIPropertyName, &deviceName);
	if (deviceName != NULL) {
		fullName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@: %@"),
						deviceName, endpointName);
	} else {
		fullName = endpointName;
	}

	/* copy the string into our buffer as UTF-8 */
	CFStringGetCString(fullName, buf, buf_len, kCFStringEncodingUTF8);

	/* clean up */
	if (fullName && !deviceName) CFRelease(fullName);
}

static int macosx_start_(struct midi_port *p)
{
	struct macosx_midi *m;
	struct macosx_midi_provider *mp;

	m  = p->userdata;
	mp = p->provider->userdata;

	if ((p->io & MIDI_INPUT)
		&& MIDIPortConnectSource(mp->portIn, m->ep, (void*)p) != noErr)
		return 0;

	if (p->io & MIDI_OUTPUT) {
		m->pl = (MIDIPacketList*)m->packet;
		m->x = NULL;
	}
	return 1;
}
static int macosx_stop_(struct midi_port *p)
{
	struct macosx_midi *m;
	struct macosx_midi_provider *mp;

	m  = p->userdata;
	mp = p->provider->userdata;

	if ((p->io & MIDI_INPUT)
		&& MIDIPortDisconnectSource(mp->portIn, m->ep) != noErr)
		return 0;

	return 1;
}

static void macosx_add_port_(struct midi_provider *p, MIDIEndpointRef ep, uint8_t inout)
{
	struct macosx_midi *m;
	struct midi_port* ptr;
	char name[55];

	if (!ep)
		return;

	ptr = NULL;
	while (midi_port_foreach(p, &ptr)) {
		m = ptr->userdata;
		if (m->ep == ep && ptr->iocap == inout) {
			ptr->mark = 0;
			return;
		}
	}

	m = mem_alloc(sizeof(struct macosx_midi));
	m->ep = ep;

	/* 55 is the maximum size for the MIDI page */
	get_ep_name(m->ep, name, 55);
	name[54] = '\0';

	midi_port_register(p, inout, name, m, free);
}

static void macosx_poll_(struct midi_provider *p)
{
	struct midi_port* ptr;
	struct macosx_midi* m;
	MIDIEndpointRef ep;
	ItemCount i;

	ItemCount num_out, num_in;

	num_out = MIDIGetNumberOfDestinations();
	num_in = MIDIGetNumberOfSources();

	midi_provider_mark_ports(p);

	for (i = 0; i < num_out; i++)
		macosx_add_port_(p, MIDIGetDestination(i), MIDI_OUTPUT);

	for (i = 0; i < num_in; i++)
		macosx_add_port_(p, MIDIGetSource(i), MIDI_INPUT);

	midi_provider_remove_marked_ports(p);
}

static void macosx_provider_destroy_(void *userdata)
{
	struct macosx_midi_provider *p = userdata;

	MIDIPortDispose(p->portOut);
	MIDIPortDispose(p->portIn);
	MIDIClientDispose(p->client);

	free(p);
}

int macosx_midi_setup(void)
{
	struct macosx_midi_provider *p;
	static const struct midi_driver driver = {
		.flags = MIDI_PORT_CAN_SCHEDULE,
		.poll = macosx_poll_,
		.enable = macosx_start_,
		.disable = macosx_stop_,
		.send = macosx_send_,
		.drain = macosx_drain_,
		.destroy_userdata = macosx_provider_destroy_,
	};

	p = mem_alloc(sizeof(*p));

	if (MIDIClientCreate(CFSTR("Schism Tracker"), NULL, NULL, &p->client) != noErr)
		goto fail_client;

	if (MIDIInputPortCreate(p->client, CFSTR("Input port"), readProc, NULL, &p->portIn) != noErr)
		goto fail_port_in;

	if (MIDIOutputPortCreate(p->client, CFSTR("Output port"), &p->portOut) != noErr)
		goto fail_port_out;

	/* TODO move statics into a structure */
	if (!midi_provider_register("Mac OS X", &driver, p))
		goto fail_provider_register;

	return 1;

	/* failure gotos :) */
fail_provider_register:
	MIDIPortDispose(p->portOut);
fail_port_out:
	MIDIPortDispose(p->portIn);
fail_port_in:
	MIDIClientDispose(p->client);
fail_client:
	free(p);
	return 0;
}
