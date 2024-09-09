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
#include "event.h"
#include "song.h"
#include "page.h"

#include <IOKit/hid/IOHIDLib.h>

/* --------------------------------------------------------- */
/* Handle Caps Lock and other oddities. Much of the HID code is
 * stolen from SDL. */

/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

static IOHIDManagerRef hid_manager = NULL;
static SDL_atomic_t is_caps_pressed = {0};

static void hid_callback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
	IOHIDElementRef elem = IOHIDValueGetElement(value);
	if (IOHIDElementGetUsagePage(elem) != kHIDPage_KeyboardOrKeypad
	    || IOHIDElementGetUsage(elem) != kHIDUsage_KeyboardCapsLock)
		return;

	const int pressed = IOHIDValueGetIntegerValue(value);
	SDL_AtomicSet(&is_caps_pressed, !!pressed);
}

static CFDictionaryRef create_hid_device(uint32_t page, uint32_t usage)
{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
		kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (dict) {
		CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
		if (number) {
			CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), number);
			CFRelease(number);
			number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
			if (number) {
				CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsageKey), number);
				CFRelease(number);
				return dict;
			}
		}
		CFRelease(dict);
	}
	return NULL;
}

static void quit_hid_callback(void)
{
	if (!hid_manager) return;

	IOHIDManagerUnscheduleFromRunLoop(hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerRegisterInputValueCallback(hid_manager, NULL, NULL);
	IOHIDManagerClose(hid_manager, 0);
	CFRelease(hid_manager);
	hid_manager = NULL;
}

static void init_hid_callback(void)
{
	hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!hid_manager) return;

	CFDictionaryRef keyboard = NULL, keypad = NULL;
	CFArrayRef matches = NULL;

	keyboard = create_hid_device(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
	if (!keyboard) goto fail;

	keypad = create_hid_device(kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad);
	if (!keypad) goto fail;

	CFDictionaryRef matches_list[] = {keyboard, keypad};
	matches = CFArrayCreate(kCFAllocatorDefault, (const void **)matches_list, 2, NULL);
	if (!matches) goto fail;

	IOHIDManagerSetDeviceMatchingMultiple(hid_manager, matches);
	IOHIDManagerRegisterInputValueCallback(hid_manager, hid_callback, NULL);
	IOHIDManagerScheduleWithRunLoop(hid_manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
	if (IOHIDManagerOpen(hid_manager, kIOHIDOptionsTypeNone) == kIOReturnSuccess) goto cleanup;

fail:
	quit_hid_callback();

cleanup:
	if (matches) CFRelease(matches);

	if (keypad) CFRelease(keypad);

	if (keyboard) CFRelease(keyboard);
}

/* --------------------------------------------------------- */

/* this gets stored at startup as the initial value of fnswitch before
 * we tamper with it, so we can restore it on shutdown */
static int ibook_helper = -1;

int macosx_sdlevent(SDL_Event *event)
{
	switch (event->type) {
	case SDL_WINDOWEVENT:
		switch (event->window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED: macosx_ibook_fnswitch(1); break;
		case SDL_WINDOWEVENT_FOCUS_LOST: macosx_ibook_fnswitch(ibook_helper); break;
		default: break;
		}
		return 1;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch (status.fix_numlock_setting) {
		case NUMLOCK_GUESS:
			/* why is this checking for ibook_helper? */
			if (ibook_helper != -1) {
				if (ACTIVE_PAGE.selected_widget > -1 && ACTIVE_PAGE.selected_widget < ACTIVE_PAGE.total_widgets
				    && ACTIVE_PAGE_WIDGET.accept_text) {
					/* text is more likely? */
					event->key.keysym.mod |= KMOD_NUM;
				} else {
					event->key.keysym.mod &= ~KMOD_NUM;
				}
			} /* otherwise honor it */
			break;
		default:
			/* other cases are handled in schism/main.c */
			break;
		}

		switch (event->key.keysym.scancode) {
		case SDL_SCANCODE_KP_ENTER:
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
			event->key.keysym.sym = SDLK_INSERT;
			break;
		default: break;
		};
		return 1;
	default: break;
	}
	return 1;
}

void macosx_sysexit(void)
{
	/* return back to default */
	if (ibook_helper != -1) macosx_ibook_fnswitch(ibook_helper);

	quit_hid_callback();
}

void macosx_sysinit(UNUSED int *pargc, UNUSED char ***pargv)
{
	/* macosx_ibook_fnswitch only sets the value if it's one of (0, 1) */
	ibook_helper = macosx_ibook_fnswitch(-1);

	init_hid_callback();
}

void macosx_get_modkey(UNUSED int *mk)
{
	int caps = SDL_AtomicGet(&is_caps_pressed);

	if (caps) status.flags |= CAPS_PRESSED;
	else status.flags &= ~CAPS_PRESSED;
}
