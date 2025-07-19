#!/usr/bin/env python3
# generates keybinds_codes.c

import zlib
import io
import dataclasses
from typing import Optional

@dataclasses.dataclass
class Keybind:
	name: str
	sdl_name: Optional[str] = None

@dataclasses.dataclass
class Keybinds:
	keybinds: list[Keybind]
	type: str
	schism_prefix: str
	sdl_prefix: str
	default_value: str

keymods = Keybinds([
	Keybind("CTRL"),
	Keybind("LCTRL"),
	Keybind("RCTRL"),
	Keybind("SHIFT"),
	Keybind("LSHIFT"),
	Keybind("RSHIFT"),
	Keybind("ALT"),
	Keybind("LALT"),
	Keybind("RALT"),
], "uint16_t", "", "KEYBIND_MOD_", "KEYBIND_MOD_NONE")

scancodes = Keybinds([
	Keybind("A"),
	Keybind("B"),
	Keybind("C"),
	Keybind("D"),
	Keybind("E"),
	Keybind("F"),
	Keybind("G"),
	Keybind("H"),
	Keybind("I"),
	Keybind("J"),
	Keybind("K"),
	Keybind("L"),
	Keybind("M"),
	Keybind("N"),
	Keybind("O"),
	Keybind("P"),
	Keybind("Q"),
	Keybind("R"),
	Keybind("S"),
	Keybind("T"),
	Keybind("U"),
	Keybind("V"),
	Keybind("W"),
	Keybind("X"),
	Keybind("Y"),
	Keybind("Z"),
	Keybind("1"),
	Keybind("2"),
	Keybind("3"),
	Keybind("4"),
	Keybind("5"),
	Keybind("6"),
	Keybind("7"),
	Keybind("8"),
	Keybind("9"),
	Keybind("0"),
	Keybind("F1"),
	Keybind("F2"),
	Keybind("F3"),
	Keybind("F4"),
	Keybind("F5"),
	Keybind("F6"),
	Keybind("F7"),
	Keybind("F8"),
	Keybind("F9"),
	Keybind("F10"),
	Keybind("F11"),
	Keybind("F12"),
	Keybind("RETURN"),
	Keybind("ENTER", sdl_name="RETURN"),
	Keybind("ESCAPE"),
	Keybind("BACKSPACE"),
	Keybind("TAB"),
	Keybind("SPACE"),
	Keybind("MINUS"),
	Keybind("EQUALS"),
	Keybind("LEFTBRACKET"),
	Keybind("RIGHTBRACKET"),
	Keybind("BACKSLASH"),
	Keybind("SEMICOLON"),
	Keybind("APOSTROPHE"),
	Keybind("GRAVE"),
	Keybind("COMMA"),
	Keybind("PERIOD"),
	Keybind("SLASH"),
	Keybind("CAPSLOCK"),
	Keybind("PRINTSCREEN"),
	Keybind("SCROLLLOCK"),
	Keybind("PAUSE"),
	Keybind("INSERT"),
	Keybind("DELETE"),
	Keybind("HOME"),
	Keybind("END"),
	Keybind("PAGEUP"),
	Keybind("PAGEDOWN"),
	Keybind("RIGHT"),
	Keybind("LEFT"),
	Keybind("DOWN"),
	Keybind("UP"),
	Keybind("NUMLOCKCLEAR"),  # numlock on PC, clear on macs
	Keybind("KP_DIVIDE"),
	Keybind("KP_MULTIPLY"),
	Keybind("KP_MINUS"),
	Keybind("KP_PLUS"),
	Keybind("KP_ENTER"),
	Keybind("KP_1"),
	Keybind("KP_2"),
	Keybind("KP_3"),
	Keybind("KP_4"),
	Keybind("KP_5"),
	Keybind("KP_6"),
	Keybind("KP_7"),
	Keybind("KP_8"),
	Keybind("KP_9"),
	Keybind("KP_0"),
	Keybind("KP_PERIOD"),
	Keybind("KP_EQUALS"),
], "schism_scancode_t", "US_", "SCHISM_SCANCODE_", "SCHISM_SCANCODE_UNKNOWN");

keycodes = Keybinds([
	Keybind("RETURN"),
	Keybind("ENTER", sdl_name="RETURN"),
	Keybind("ESCAPE"),
	Keybind("BACKSPACE"),
	Keybind("TAB"),
	Keybind("SPACE"),
	Keybind("EXCLAIM"),
	Keybind("QUOTEDBL"),
	Keybind("HASH"),
	Keybind("PERCENT"),
	Keybind("DOLLAR"),
	Keybind("AMPERSAND"),
	Keybind("QUOTE"),
	Keybind("LEFTPAREN"),
	Keybind("RIGHTPAREN"),
	Keybind("ASTERISK"),
	Keybind("PLUS"),
	Keybind("COMMA"),
	Keybind("MINUS"),
	Keybind("PERIOD"),
	Keybind("SLASH"),
	Keybind("0"),
	Keybind("1"),
	Keybind("2"),
	Keybind("3"),
	Keybind("4"),
	Keybind("5"),
	Keybind("6"),
	Keybind("7"),
	Keybind("8"),
	Keybind("9"),
	Keybind("COLON"),
	Keybind("SEMICOLON"),
	Keybind("LESS"),
	Keybind("EQUALS"),
	Keybind("GREATER"),
	Keybind("QUESTION"),
	Keybind("AT"),
	Keybind("LEFTBRACKET"),
	Keybind("BACKSLASH"),
	Keybind("RIGHTBRACKET"),
	Keybind("CARET"),
	Keybind("UNDERSCORE"),
	Keybind("BACKQUOTE"),
	Keybind("a"),
	Keybind("b"),
	Keybind("c"),
	Keybind("d"),
	Keybind("e"),
	Keybind("f"),
	Keybind("g"),
	Keybind("h"),
	Keybind("i"),
	Keybind("j"),
	Keybind("k"),
	Keybind("l"),
	Keybind("m"),
	Keybind("n"),
	Keybind("o"),
	Keybind("p"),
	Keybind("q"),
	Keybind("r"),
	Keybind("s"),
	Keybind("t"),
	Keybind("u"),
	Keybind("v"),
	Keybind("w"),
	Keybind("x"),
	Keybind("y"),
	Keybind("z"),
	Keybind("CAPSLOCK"),
	Keybind("F1"),
	Keybind("F2"),
	Keybind("F3"),
	Keybind("F4"),
	Keybind("F5"),
	Keybind("F6"),
	Keybind("F7"),
	Keybind("F8"),
	Keybind("F9"),
	Keybind("F10"),
	Keybind("F11"),
	Keybind("F12"),
	Keybind("PRINTSCREEN"),
	Keybind("SCROLLLOCK"),
	Keybind("PAUSE"),
	Keybind("INSERT"),
	Keybind("HOME"),
	Keybind("PAGEUP"),
	Keybind("DELETE"),
	Keybind("END"),
	Keybind("PAGEDOWN"),
	Keybind("RIGHT"),
	Keybind("LEFT"),
	Keybind("DOWN"),
	Keybind("UP"),
	Keybind("NUMLOCKCLEAR"),
	Keybind("KP_DIVIDE"),
	Keybind("KP_MULTIPLY"),
	Keybind("KP_MINUS"),
	Keybind("KP_PLUS"),
	Keybind("KP_ENTER"),
	Keybind("KP_1"),
	Keybind("KP_2"),
	Keybind("KP_3"),
	Keybind("KP_4"),
	Keybind("KP_5"),
	Keybind("KP_6"),
	Keybind("KP_7"),
	Keybind("KP_8"),
	Keybind("KP_9"),
	Keybind("KP_0"),
	Keybind("KP_PERIOD"),
	Keybind("APPLICATION"),
	Keybind("POWER"),
	Keybind("KP_EQUALS"),
	Keybind("F13"),
	Keybind("F14"),
	Keybind("F15"),
	Keybind("F16"),
	Keybind("F17"),
	Keybind("F18"),
	Keybind("F19"),
	Keybind("F20"),
	Keybind("F21"),
	Keybind("F22"),
	Keybind("F23"),
	Keybind("F24"),
	Keybind("EXECUTE"),
	Keybind("HELP"),
	Keybind("MENU"),
	Keybind("SELECT"),
	Keybind("STOP"),
	Keybind("AGAIN"),
	Keybind("UNDO"),
	Keybind("CUT"),
	Keybind("COPY"),
	Keybind("PASTE"),
	Keybind("FIND"),
	Keybind("MUTE"),
	Keybind("VOLUMEUP"),
	Keybind("VOLUMEDOWN"),
	Keybind("KP_COMMA"),
	Keybind("KP_EQUALSAS400"),
	Keybind("ALTERASE"),
	Keybind("SYSREQ"),
	Keybind("CANCEL"),
	Keybind("CLEAR"),
	Keybind("PRIOR"),
	Keybind("RETURN2"),
	Keybind("SEPARATOR"),
	Keybind("OUT"),
	Keybind("OPER"),
	Keybind("CLEARAGAIN"),
	Keybind("CRSEL"),
	Keybind("EXSEL"),
	Keybind("KP_00"),
	Keybind("KP_000"),
	Keybind("THOUSANDSSEPARATOR"),
	Keybind("DECIMALSEPARATOR"),
	Keybind("CURRENCYUNIT"),
	Keybind("CURRENCYSUBUNIT"),
	Keybind("KP_LEFTPAREN"),
	Keybind("KP_RIGHTPAREN"),
	Keybind("KP_LEFTBRACE"),
	Keybind("KP_RIGHTBRACE"),
	Keybind("KP_TAB"),
	Keybind("KP_BACKSPACE"),
	Keybind("KP_A"),
	Keybind("KP_B"),
	Keybind("KP_C"),
	Keybind("KP_D"),
	Keybind("KP_E"),
	Keybind("KP_F"),
	Keybind("KP_XOR"),
	Keybind("KP_POWER"),
	Keybind("KP_PERCENT"),
	Keybind("KP_LESS"),
	Keybind("KP_GREATER"),
	Keybind("KP_AMPERSAND"),
	Keybind("KP_DBLAMPERSAND"),
	Keybind("KP_VERTICALBAR"),
	Keybind("KP_DBLVERTICALBAR"),
	Keybind("KP_COLON"),
	Keybind("KP_HASH"),
	Keybind("KP_SPACE"),
	Keybind("KP_AT"),
	Keybind("KP_EXCLAM"),
	Keybind("KP_MEMSTORE"),
	Keybind("KP_MEMRECALL"),
	Keybind("KP_MEMCLEAR"),
	Keybind("KP_MEMADD"),
	Keybind("KP_MEMSUBTRACT"),
	Keybind("KP_MEMMULTIPLY"),
	Keybind("KP_MEMDIVIDE"),
	Keybind("KP_PLUSMINUS"),
	Keybind("KP_CLEAR"),
	Keybind("KP_CLEARENTRY"),
	Keybind("KP_BINARY"),
	Keybind("KP_OCTAL"),
	Keybind("KP_DECIMAL"),
	Keybind("KP_HEXADECIMAL"),
	Keybind("RGUI"),
	#Keybind("MODE"),
	#Keybind("AUDIONEXT"),
	#Keybind("AUDIOPREV"),
	#Keybind("AUDIOSTOP"),
	#Keybind("AUDIOPLAY"),
	#Keybind("AUDIOMUTE"),
	#Keybind("MEDIASELECT"),
	#Keybind("WWW"),
	#Keybind("MAIL"),
	#Keybind("CALCULATOR"),
	#Keybind("COMPUTER"),
	#Keybind("AC_SEARCH"),
	#Keybind("AC_HOME"),
	#Keybind("AC_BACK"),
	#Keybind("AC_FORWARD"),
	#Keybind("AC_STOP"),
	#Keybind("AC_REFRESH"),
	#Keybind("AC_BOOKMARKS"),
	#Keybind("BRIGHTNESSDOWN"),
	#Keybind("BRIGHTNESSUP"),
	#Keybind("DISPLAYSWITCH"),
	#Keybind("KBDILLUMTOGGLE"),
	#Keybind("KBDILLUMDOWN"),
	#Keybind("KBDILLUMUP"),
	#Keybind("EJECT"),
	#Keybind("SLEEP"),
	#Keybind("APP1"),
	#Keybind("APP2"),
	#Keybind("AUDIOREWIND"),
	#Keybind("AUDIOFASTFORWARD"),
	#Keybind("SOFTLEFT"),
	#Keybind("SOFTRIGHT"),
	#Keybind("CALL"),
	#Keybind("ENDCALL"),
], "schism_keysym_t", "", "SCHISM_KEYSYM_", "SCHISM_KEYSYM_UNKNOWN")

def generate_function(file: io.TextIOWrapper, name: str, keybinds: Keybinds):
	f.write("int %s(const char *name, %s *ret)\n" % (name, keybinds.type))
	f.write("{\n")
	f.write("\tchar *casefolded_name = charset_case_fold(name, CHARSET_UTF8);\n");
	f.write("\tuint32_t crc = crc32b(casefolded_name);\n");
	f.write("\tfree(casefolded_name);\n\n")
	f.write("\tswitch (crc) {\n")

	for keybind in keybinds.keybinds:
		name = (keybinds.schism_prefix + keybind.name).casefold()

		f.write("\tcase 0x%08x: /* %s */\n" % (zlib.crc32(name.encode('utf-8')), name))
		f.write("\t\t*ret = %s%s;\n" % (keybinds.sdl_prefix, keybind.sdl_name if not keybind.sdl_name is None else keybind.name))
		f.write("\t\treturn 1;\n")

	f.write("\tdefault:\n")
	f.write("\t\tbreak;\n")
	f.write("\t}\n\n")

	f.write("\t*ret = %s;\n" % keybinds.default_value)
	f.write("\treturn 0;\n")

	f.write("}\n\n")

with open("../schism/keybinds_codes.c", "w") as f:
	f.write("""/*
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
 */\n\n""")

	for i in ["it.h", "util.h", "charset.h", "keybinds.h"]:
		f.write("#include \"%s\"\n" % i)
	f.write("\n")

	f.write("/* do NOT edit anything in this file, it's generated by `scripts/genkeybinds.py`.\n")
	f.write(" * edit that script and regenerate instead of editing this file */\n\n")

	generate_function(f, "keybinds_parse_scancode", scancodes)
	generate_function(f, "keybinds_parse_modkey", keymods)
	generate_function(f, "keybinds_parse_keycode", keycodes)
