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

#include "midi.h"

#include "mixer.h"
#include "util.h"

#ifdef MACOSX

#include <CoreServices/CoreServices.h>
#include <CoreMIDI/MIDIServices.h>
#include <CoreAudio/HostTime.h>

static MIDIClientRef    client = NULL;
static MIDIPortRef      portIn = NULL;
static MIDIPortRef      portOut = NULL;

static int max_outputs = 0;
static int max_inputs = 0;

struct macosx_midi {
        char *name;
        MIDIEndpointRef ep;
        unsigned char packet[1024];
        MIDIPacketList *pl;
        MIDIPacket *x;
};

static void readProc(const MIDIPacketList *np, UNUSED void *rc, void *crc)
{
        struct midi_port *p;
        struct macosx_midi *m;
        MIDIPacket *x;
        unsigned long i;

        p = (struct midi_port *)crc;
        m = (struct macosx_midi *)p->userdata;

        x = (MIDIPacket*)&np->packet[0];
        for (i = 0; i < np->numPackets; i++) {
                midi_received_cb(p, x->data, x->length);
                x = MIDIPacketNext(x);
        }
}
static void _macosx_send(struct midi_port *p, const unsigned char *data,
                                unsigned int len, unsigned int delay)
{
        struct macosx_midi *m;

        m = (struct macosx_midi *)p->userdata;
        if (!m->x) {
                m->x = MIDIPacketListInit(m->pl);
        }

        m->x = MIDIPacketListAdd(m->pl, sizeof(m->packet),
                        m->x, (MIDITimeStamp)AudioConvertNanosToHostTime(
                        AudioConvertHostTimeToNanos(AudioGetCurrentHostTime()) + (1000000*delay)), /* msec to nsec? */
                        len, data);
}
static void _macosx_drain(struct midi_port *p)
{
        struct macosx_midi *m;

        m = (struct macosx_midi *)p->userdata;
        if (m->x) {
                MIDISend(portOut, m->ep, m->pl);
                m->x = NULL;
        }
}

/* lifted from portmidi */
static char *get_ep_name(MIDIEndpointRef ep)
{
        MIDIEntityRef entity;
        MIDIDeviceRef device;
        CFStringRef endpointName = NULL, deviceName = NULL, fullName = NULL;
        CFStringEncoding defaultEncoding;
        char* newName;

        /* get the default string encoding */
        defaultEncoding = CFStringGetSystemEncoding();

        /* get the entity and device info */
        MIDIEndpointGetEntity(ep, &entity);
        MIDIEntityGetDevice(entity, &device);

        /* create the nicely formated name */
        MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &endpointName);
        MIDIObjectGetStringProperty(device, kMIDIPropertyName, &deviceName);
        if (deviceName != NULL) {
                fullName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@: %@"),
                                                deviceName, endpointName);
        } else {
                fullName = endpointName;
        }

        /* copy the string into our buffer */
        newName = (char*)mem_alloc(CFStringGetLength(fullName) + 1);
        CFStringGetCString(fullName, newName, CFStringGetLength(fullName) + 1,
                        defaultEncoding);

        /* clean up */
        if (endpointName) CFRelease(endpointName);
        if (deviceName) CFRelease(deviceName);
        if (fullName) CFRelease(fullName);

        return newName;
}

static int _macosx_start(struct midi_port *p)
{
        struct macosx_midi *m;
        m = (struct macosx_midi *)p->userdata;

        if (p->io & MIDI_INPUT
        && MIDIPortConnectSource(portIn, m->ep, (void*)p) != noErr) {
                return 0;
        }

        if (p->io & MIDI_OUTPUT) {
                m->pl = (MIDIPacketList*)m->packet;
                m->x = NULL;
        }
        return 1;
}
static int _macosx_stop(struct midi_port *p)
{
        struct macosx_midi *m;
        m = (struct macosx_midi *)p->userdata;
        if (p->io & MIDI_INPUT
        && MIDIPortDisconnectSource(portIn, m->ep) != noErr) {
                return 0;
        }
        return 1;
}

static void _macosx_poll(struct midi_provider *p)
{
        struct macosx_midi *data;
        MIDIEndpointRef ep;
        int i;

        int num_out, num_in;

        num_out = MIDIGetNumberOfDestinations();
        num_in = MIDIGetNumberOfSources();

        for (i = max_outputs; i < num_out; i++) {
                ep = MIDIGetDestination(i);
                if (!ep) continue;
                data = mem_alloc(sizeof(struct macosx_midi));
                memcpy(&data->ep, &ep, sizeof(ep));
                data->name = get_ep_name(ep);
                midi_port_register(p, MIDI_OUTPUT, data->name, data, 1);
        }
        max_outputs = i;


        for (i = max_inputs; i < num_in; i++) {
                ep = MIDIGetSource(i);
                if (!ep) continue;
                data = mem_alloc(sizeof(struct macosx_midi));
                memcpy(&data->ep, &ep, sizeof(ep));
                data->name = get_ep_name(ep);
                midi_port_register(p, MIDI_INPUT, data->name, data, 1);
        }
        max_inputs = i;

}

int macosx_midi_setup(void)
{
        static struct midi_driver driver;

        memset(&driver,0,sizeof(driver));
        driver.flags = MIDI_PORT_CAN_SCHEDULE;
        driver.poll = _macosx_poll;
        driver.thread = NULL;
        driver.enable = _macosx_start;
        driver.disable = _macosx_stop;
        driver.send = _macosx_send;
        driver.drain = _macosx_drain;

        if (MIDIClientCreate(CFSTR("Schism Tracker"), NULL, NULL, &client) != noErr) {
                return 0;
        }
        if (MIDIInputPortCreate(client, CFSTR("Input port"), readProc, NULL, &portIn) != noErr) {
                return 0;
        }
        if (MIDIOutputPortCreate(client, CFSTR("Output port"), &portOut) != noErr) {
                return 0;
        }

        if (!midi_provider_register("Mac OS X", &driver)) return 0;

        return 1;
}

#endif
