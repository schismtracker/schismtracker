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

#include "it.h"
#include "osdefs.h"
#include "events.h"
#include "song.h"
#include "page.h"
#include "widget.h"
#include "mem.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>

/* --------------------------------------------------------- */
/* Handle Caps Lock and other oddities; Cocoa doesn't send very
 * specific key events so we have to do this manually through the
 * HID library. Annoying. */

// #define SCHISM_MACOSXHID_DEBUG

struct hid_item_data {
	IOHIDDeviceInterface **interface; // hm

	struct {
		IOHIDElementCookie caps_lock;
	} cookies;
};

struct hid_item_node {
	struct hid_item_data data;
	struct hid_item_node *next;
};

static struct hid_item_node *hid_item_list = NULL;

static void hid_item_insert(struct hid_item_data *data)
{
	struct hid_item_node *node = mem_alloc(sizeof(*node));
	memcpy(&node->data, data, sizeof(node->data));
	node->next = hid_item_list;
	hid_item_list = node;
}

static void hid_item_free(void)
{
	struct hid_item_node* temp;

	while (hid_item_list) {
		temp = hid_item_list;
		hid_item_list = hid_item_list->next;
		if (temp->data.interface) {
			(*temp->data.interface)->close(temp->data.interface);
			(*temp->data.interface)->Release(temp->data.interface);
		}
		free(temp);
	}
}

static CFDictionaryRef create_hid_device(uint32_t page, uint32_t usage) {
	CFMutableDictionaryRef dict = IOServiceMatching(kIOHIDDeviceKey);
	if (dict) {
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
		if (number) {
			CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsagePageKey), number);
			CFRelease(number);
			number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
			if (number) {
				CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsageKey), number);
				CFRelease(number);
				CFRetain(dict);
				return dict;
			}
		}
		CFRelease(dict);
	}
	return NULL;
}

static void quit_hid_callback(void) {
	hid_item_free();
}

static void process_hid_element(const void *value, struct hid_item_data *data)
{
	if (CFGetTypeID(value) != CFDictionaryGetTypeID()) {
#ifdef SCHISM_MACOSXHID_DEBUG
		printf("MACOSX HID HID element isn't a dictionary?\n");
#endif
		return; // err
	}

	CFDictionaryRef dict = (CFDictionaryRef)value;

	uint32_t element_type = 0, usage_page = 0, usage = 0, cookie = 0;

	{
		CFTypeRef type_ref;

		// element type
		type_ref = CFDictionaryGetValue(dict, CFSTR(kIOHIDElementTypeKey));
		if (type_ref) CFNumberGetValue(type_ref, kCFNumberIntType, &element_type);

		// usage page
		type_ref = CFDictionaryGetValue(dict, CFSTR(kIOHIDElementUsagePageKey));
		if (type_ref) CFNumberGetValue(type_ref, kCFNumberIntType, &usage_page);

		// usage page
		type_ref = CFDictionaryGetValue(dict, CFSTR(kIOHIDElementUsageKey));
		if (type_ref) CFNumberGetValue(type_ref, kCFNumberIntType, &usage);

		// cookie! (mmm delicious...)
		type_ref = CFDictionaryGetValue(dict, CFSTR(kIOHIDElementCookieKey));
		if (type_ref) CFNumberGetValue(type_ref, kCFNumberIntType, &cookie);
	}

#ifdef SCHISM_MACOSXHID_DEBUG
	printf("got element with type %x, usage page %x, usage %x, and cookie %x\n", element_type, usage_page, usage, cookie);
#endif

	switch (element_type) {
	// Mac OS X defines these as buttons, which makes sense
	// considering a keyboard is in essense just a giant
	// basket case of buttons...
	case kIOHIDElementTypeInput_Button:
		switch (usage_page) {
		case kHIDUsage_GD_Keyboard:
		case kHIDUsage_GD_Keypad:
			switch (usage) {
			case kHIDUsage_KeyboardCapsLock:
#ifdef SCHISM_MACOSXHID_DEBUG
				puts("MACOSX HID: found caps lock key");
#endif
				data->cookies.caps_lock = (IOHIDElementCookie)cookie;
				break;
			default:
				// don't care about other cookies for now
				break;
			}
			break;
		default:
			// should never ever happen
			break;
		}
		break;
	case kIOHIDElementTypeCollection: {
		// ugh
		CFTypeRef element = CFDictionaryGetValue(dict, CFSTR(kIOHIDElementKey));
		if (!element || CFGetTypeID(element) != CFArrayGetTypeID())
			break; // what?

		for (int i = 0; i < CFArrayGetCount(element); i++)
			process_hid_element(CFArrayGetValueAtIndex(element, i), data);

		break;
	}
	default:
		break;
	}
}

static void add_hid_device(io_object_t hid_device)
{
	struct hid_item_data item = {0};

	CFMutableDictionaryRef hid_properties = NULL;

	kern_return_t result = IORegistryEntryCreateCFProperties(hid_device, &hid_properties, kCFAllocatorDefault, kNilOptions);
	if (result != KERN_SUCCESS)
		goto fail;

	// fill in the interface
	{
		SInt32 score = 0; // dunno what this is for

		// receive the interface (this is like IUnknown * on windows or something)
		IOCFPlugInInterface **plugin_interface = NULL;

		result = IOCreatePlugInInterfaceForService(hid_device, kIOHIDDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin_interface, &score);

		if (result != KERN_SUCCESS) {
#ifdef SCHISM_MACOSXHID_DEBUG
			printf("MACOSX HID IOCreatePlugInInterfaceForService failed: %d\n", result);
#endif
			goto fail;
		}

		// yapfest to get the real interface we want
		if ((*plugin_interface)->QueryInterface(plugin_interface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (void *)&(item.interface)) == S_OK) {
			// and NOW open it, the option bits are unused I think ?
			result = (*item.interface)->open(item.interface, 0);
			if (result != kIOReturnSuccess) {
#ifdef SCHISM_MACOSXHID_DEBUG
				printf("MACOSX HID opening interface failed failed: %d\n", result);
#endif
				IODestroyPlugInInterface((IOCFPlugInInterface **)item.interface);
				goto fail;
			}
		} else {
			IODestroyPlugInInterface(plugin_interface);
			goto fail;
		}
	}

	// now fill in the cookies
	{
		// search for the caps lock element...
		CFTypeRef element = CFDictionaryGetValue(hid_properties, CFSTR(kIOHIDElementKey));
		if (!element || CFGetTypeID(element) != CFArrayGetTypeID()) {
#ifdef SCHISM_MACOSXHID_DEBUG
			printf("MACOSX HID no elements?\n");
#endif
			goto fail; // whoops
		}

		for (int i = 0; i < CFArrayGetCount(element); i++)
			process_hid_element(CFArrayGetValueAtIndex(element, i), &item);
	}

	hid_item_insert(&item);

fail:
	if (hid_properties)
		CFRelease(hid_properties);
}

static void add_hid_devices(mach_port_t master_port, uint32_t page, uint32_t usage)
{
	IOReturn res;
	io_iterator_t hid_iterator = 0;

	CFDictionaryRef device = create_hid_device(page, usage);
	if (!device)
		goto end;

	res = IOServiceGetMatchingServices(master_port, device, &hid_iterator);

	if (res != kIOReturnSuccess) {
#ifdef SCHISM_MACOSXHID_DEBUG
		printf("MACOSX HID IOServiceGetMatchingServices: %d\n", res);
#endif
		goto end;
	}

	if (!hid_iterator) // paranoia at its finest
		goto end;

	{
		// iterate over each object and add it to our list of HID devices
		io_object_t hid_object = 0;

		while ((hid_object = IOIteratorNext(hid_iterator))) {
#ifdef SCHISM_MACOSXHID_DEBUG
			printf("MACOSX HID device found...\n");
#endif
			add_hid_device(hid_object);
			IOObjectRelease(hid_object);
		}
	}

end:
	if (device)
		CFRelease(device);

	if (hid_iterator)
		IOObjectRelease(hid_iterator);
}

static void init_hid_callback(void) {
	kern_return_t res;
	mach_port_t master_port = MACH_PORT_NULL;

	res = IOMasterPort(MACH_PORT_NULL, &master_port);

	if (res != kIOReturnSuccess) {
#ifdef SCHISM_MACOSXHID_DEBUG
		printf("MACOSX HID failed to get master port: %d\n", res);
#endif
		goto fail;
	}

	add_hid_devices(master_port, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
	add_hid_devices(master_port, kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad);

fail:
	return;
}

/* --------------------------------------------------------- */

/* this gets stored at startup as the initial value of fnswitch before
 * we tamper with it, so we can restore it on shutdown */
static int ibook_helper = -1;

int macosx_event(schism_event_t *event)
{
	switch (event->type) {
	case SCHISM_WINDOWEVENT_FOCUS_GAINED:
		macosx_ibook_fnswitch(1);
		return 1;
	case SCHISM_WINDOWEVENT_FOCUS_LOST:
		macosx_ibook_fnswitch(ibook_helper);
		return 1;
	case SCHISM_KEYDOWN:
	case SCHISM_KEYUP:
		switch (status.fix_numlock_setting) {
		case NUMLOCK_GUESS:
			/* why is this checking for ibook_helper? */
			if (ibook_helper != -1) {
				if (ACTIVE_PAGE.selected_widget > -1
					&& ACTIVE_PAGE.selected_widget < ACTIVE_PAGE.total_widgets
					&& ACTIVE_PAGE_WIDGET.accept_text) {
					/* text is more likely? */
					event->key.mod |= SCHISM_KEYMOD_NUM;
				} else {
					event->key.mod &= ~SCHISM_KEYMOD_NUM;
				}
			} /* otherwise honor it */
			break;
		default:
			/* other cases are handled in schism/main.c */
			break;
		}

		switch (event->key.scancode) {
		case SCHISM_SCANCODE_KP_ENTER:
			/* On portables, the regular Insert key
			 * isn't available. This is equivalent to
			 * pressing Fn-Return, which just so happens
			 * to be a "de facto" Insert in mac land.
			 * However, on external keyboards this causes
			 * a real keypad enter to get eaten by this
			 * function as well. IMO it's more important
			 * for now that portable users can actually
			 * have an Insert key.
			 *
			 *   - paper */
			event->key.sym = SCHISM_KEYSYM_INSERT;
			break;
		default:
			break;
		};
		return 1;
	default:
		break;
	}
	return 1;
}

void macosx_sysexit(void) {
	/* return back to default */
	if (ibook_helper != -1)
		macosx_ibook_fnswitch(ibook_helper);

	quit_hid_callback();
}

void macosx_sysinit(SCHISM_UNUSED int *pargc, SCHISM_UNUSED char ***pargv) {
	/* macosx_ibook_fnswitch only sets the value if it's one of (0, 1) */
	ibook_helper = macosx_ibook_fnswitch(-1);

	init_hid_callback();
}

void macosx_get_modkey(schism_keymod_t *mk) {
	int caps_pressed = 0;

	struct hid_item_node *node;

	for (node = hid_item_list; node; node = node->next) {
		IOHIDEventStruct event;

#ifdef SCHISM_MACOSXHID_DEBUG
		printf("MACOSX HID: checking for mods on HID item\n");
#endif

		IOReturn res = (*node->data.interface)->getElementValue(node->data.interface, node->data.cookies.caps_lock, &event);

		if (res == kIOReturnSuccess)
			caps_pressed |= !!(event.value);

#ifdef SCHISM_MACOSXHID_DEBUG
		printf("MACOSX HID getElementValue: %d\n", res);
#endif
	}

#ifdef SCHISM_MACOSXHID_DEBUG
	printf("was caps pressed?: %s\n", caps_pressed ? "yes" : "no");
#endif

	(*mk) &= ~SCHISM_KEYMOD_CAPS_PRESSED;

	if (caps_pressed)
		(*mk) |= SCHISM_KEYMOD_CAPS_PRESSED;
}

void macosx_show_message_box(const char *title, const char *text)
{
	CFStringRef cfs_title, cfs_text;
	CFOptionFlags flags;

	cfs_title = CFStringCreateWithCString(kCFAllocatorDefault, title, kCFStringEncodingUTF8);
	if (cfs_title) {
		cfs_text = CFStringCreateWithCString(kCFAllocatorDefault, text, kCFStringEncodingUTF8);
		if (cfs_text) {
			CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel, NULL, NULL, NULL, cfs_title, cfs_text, NULL, NULL, NULL, &flags);
			CFRelease(cfs_text);
		}
		CFRelease(cfs_title);
	}
}
