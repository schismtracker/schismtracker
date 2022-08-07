#include <stdio.h>
#include <stdint.h>
#include <SDL.h>

#include "util.h"
#include "tree.h"


/*
TODO:
string descriptions for joystick and midi keyboard events
joystick axes and hats should emit synthetic repeated 'press' events
come up with an api for handling unicode data
velocity for joystick and midi
should really be keeping the trees balanced... meh
less stupid data structure than a linked list for the keysym translator?
put the keymaps themselves into a tree of some sort, indexed by name.
	this way, it's possible to bind a key to change the current keymap
	-- e.g. bind Ctrl-X to a "set keymap" function with data="Ctrl-X map"
need to make a list of all the things that can have associated keymaps...



All of the modifier keys MIGHT additionally be presentable as keys themselves.
(e.g. for FT2 style playback control bindings)
However, if the platform doesn't support this, too bad.

For the keyboard, most of the time the 'keycode' is a plain SDL keysym value. However, if the
keysym was zero, the scancode is used instead, with the high bit set. Most of the time SDL does
provide a keysym, but for some weird keys it might not. Scancodes are inherently non-portable,
so this is only to allow extra keys to be bound to something.
If the entire keycode value is zero, then we have no idea what key was pressed.
(This happens for input methods e.g. scim that only send a unicode character.)

For instrument list keyjazz, keydown events should keep track of the channel that got
assigned to each note, and keyup should look up that channel and send a keyoff.



Keymap files are split into sections denoted by [brackets]
Each section defines a specific keymap.

Note that this file can use the same parser as the config file.

Widgets and pages use keymaps with certain names by default, but you can define keymaps
with pretty much any name you want.

the event parser is fairly liberal and can understand many different representations of keys.
some examples follow:


; the Global map is always active, and receives all events not handled by a prior keymap
[Global]
F5=song_start_set_infopage
Ctrl+F5 = song_start
F8=song_stop
kp_multiply = set_octave +1
MIDI/1 #87 = song_loop_current_pattern
; following event is actually the fourth button on the second joystick, but in numeric format. doing things
; this way is supported but not recommended, and is really more for debugging/troubleshooting than anything.
@2/2 #3 = song_loop_pattern
keyboard/0 shift+f9 = page_switch message_editor
f9=page_switch load_module
f10=page_switch save_module
^S = song_save
escape = main_menu_toggle

[Pattern Editor]
M-q = pat_transpose +1
M-a = pat_transpose -1
M-S-q = pat_transpose +12
M-S-a = pat_transpose -12

[Sample List]
keyboard/0 meta+A = smp_sign_convert


Keymap lookup order: (prefix) | widget -> page -> global
The "prefix" map is user-defined by the kmap_set_prefix binding, and is normally empty. This map can be used
to implement multi-key events. For example:

[Global]
Ctrl-X = kmap_set_prefix ctrlx_map

[ctrlx_map]
Ctrl-S = song_save
Ctrl-C = quit

This effectively binds the Ctrl-X keys like Emacs, and also allows for "regular" bindings of those keys in
other maps (e.g. Ctrl-C can still be bound to centralise cursor, and so forth)
If a prefix map is active, no other keymaps are searched; in addition, prefix maps only last for one key.

	-- if (prefix_map && ev.bits.release) clear_prefix_map();



Additionally, a keymap can "inherit" keys from another map as follows. In this example, events not handled by
the "hamster" keymap are checked against the pattern editor map:

[hamster]
@inherit = Pattern Editor

One could conceivably define a key in the Pattern Editor map to load the hamster map,
and a reciprocating key in the monsquaz map that changes it back.
(FT2's "record mode" -- as well as IT's own capslock+key behavior -- could be implemented this way.)

* Need a function to do this: it should replace the keymap that owns the key that was pressed to trigger
the function. That is, if the keymap replace was bound to a key in the Pattern Editor map, the new keymap
loaded replaces the pointer that was previously pointing to the Pattern Editor map.
This way each keymap "layer" is independent and none of them can interfere with each other.


Somehow the keymap bindings need to know the context they're being called in.
For example, it would be entirely inappropriate to bind the left arrow to a thumbbar value adjust function
from within the pattern editor keymap.

*/

// ------------------------------------------------------------------------------------------------------------

// Superficially similar to SDL's event structure, but packed much tighter.
#pragma pack(push, 1)
typedef union {
	struct {
		unsigned int dev_type :  4; // SKDEV_TYPE_whatever
		unsigned int dev_id   :  4; // which device? (1->n; 0 is a pseudo "all" device)

		// note: not all "press" events have a corresponding "release"
		unsigned int release  :  1; // 1 for key-up

		// next three fields are only relevant for the pc keyboard
		unsigned int repeat   :  1; // 1 for synthetic key-repeat
		unsigned int unicode  :  1; // 1 if character maps to printable unicode
		unsigned int modifier :  5; // ctrl, alt, shift

		unsigned int keycode  : 16; // keyboard keysym/scancode
	} bits;
	uint32_t ival;
} isysev_t;
#pragma pack(pop)


// Device types (we can have 16 of these)
enum {
	SKDEV_TYPE_PCKEYBOARD,
	SKDEV_TYPE_MIDI,
	SKDEV_TYPE_JOYSTICK,
	// other device IDs are reserved
	SKDEV_TYPE_SENTINEL,
};

// Device IDs
// #0 is reserved as a sort of catch-all, to allow for binding the same event on all connected
// devices of a given type.
enum {
	SKDEV_ID_ANY = 0,
	SKDEV_ID_MAX = 15,
};


// Keyboard modifier bits
enum {
	SKMODE_CTRL  = 1 << 0,
	SKMODE_ALT   = 1 << 1,
	SKMODE_SHIFT = 1 << 2,
};

// Keycode flag bits (currently only used for PC keyboard)
enum {
	SKCODE_PCK_SCANCODE = 0x8000,

	SKCODE_MAX = 0xffff,
};


// Joystick limits (should be way more than enough)
// The event loop maintains a table of SKDEV_ID_MAX * MAX_JS_AXES + SKDEV_ID_MAX * MAX_JS_HATS
// in order to identify keyup and repeat events.

#define MAX_JS_BUTTONS 256
#define MAX_JS_AXES 64
#define MAX_JS_HATS 64
#define MAX_JS_BALLS 64

// Threshold values from ZSNES
#define JS_AXIS_THRESHOLD 16384
#define JS_BALL_THRESHOLD 100

enum { JS_AXIS_NEG, JS_AXIS_POS }; // for axes
enum { JS_DIR_UP, JS_DIR_DOWN, JS_DIR_LEFT, JS_DIR_RIGHT }; // for hats/balls
#define JS_BUTTON_TO_KEYCODE(n)     (n)
#define JS_AXIS_TO_KEYCODE(n, dir)  (2 * (n) + (dir) + MAX_JS_BUTTONS)
#define JS_HAT_TO_KEYCODE(n, dir)   (4 * (n) + (dir) + MAX_JS_BUTTONS + 2 * MAX_JS_AXES)
#define JS_BALL_TO_KEYCODE(n, dir)  (4 * (n) + (dir) + MAX_JS_BUTTONS + 2 * MAX_JS_AXES + 4 * MAX_JS_HATS)
#if (JS_BALL_TO_KEYCODE(MAX_JS_BALLS, 0) > 65535)
# error Joystick limits are too large!
#endif

// 8 chars max
static const char *skdev_names[] = {
	"keyboard",
	"midi",
	"joystick",
};

// ------------------------------------------------------------------------------------------------------------

// this struct sucks
typedef struct keytab {
	int code;
	const char *name;
	struct keytab *next;
} keytab_t;

static keytab_t *keytab = NULL;

static void key_add(int code, const char *name)
{
	keytab_t *k = mem_alloc(sizeof(keytab_t));
	k->code = code;
	k->name = name;
	k->next = keytab;
	keytab = k;
}


static const char *keytab_code_to_name(int keycode)
{
	keytab_t *k;

	if (!(keycode & SKCODE_PCK_SCANCODE))
		for (k = keytab; k; k = k->next)
			if (k->code == keycode)
				return k->name;
	return NULL;
}

static int keytab_name_to_code(const char *keyname)
{
	keytab_t *k;

	if (!keyname[0])
		return 0;
	for (k = keytab; k; k = k->next)
		if (strcasecmp(k->name, keyname) == 0)
			return k->code;
	return 0;
}


static void keytab_free(void)
{
	keytab_t *k, *prev = NULL;
	for (k = keytab; k; k = k->next) {
		if (prev)
			free(prev);
		prev = k;
	}
	if (prev)
		free(prev);
}

static void keytab_init(void)
{
	int n;

	// these strings should be < 15 chars, and should not start with a hash mark ('#')
	static struct {
		int code;
		const char *name;
	} keys[] = {
		{SDLK_BACKSPACE, "Backspace"},
		{SDLK_TAB, "Tab"},
		{SDLK_CLEAR, "Clear"},
		{SDLK_RETURN, "Return"},
		{SDLK_PAUSE, "Pause"},
		{SDLK_ESCAPE, "Escape"},
		{SDLK_SPACE, "Space"},
		{SDLK_EXCLAIM, "Exclaim"},
		{SDLK_QUOTEDBL, "QuoteDbl"},
		{SDLK_HASH, "Hash"},
		{SDLK_DOLLAR, "Dollar"},
		{SDLK_AMPERSAND, "Ampersand"},
		{SDLK_QUOTE, "Quote"},
		{SDLK_LEFTPAREN, "LeftParen"},
		{SDLK_RIGHTPAREN, "RightParen"},
		{SDLK_ASTERISK, "Asterisk"},
		{SDLK_PLUS, "Plus"},
		{SDLK_COMMA, "Comma"},
		{SDLK_MINUS, "Minus"},
		{SDLK_PERIOD, "Period"},
		{SDLK_SLASH, "Slash"},
		{SDLK_0, "0"},
		{SDLK_1, "1"},
		{SDLK_2, "2"},
		{SDLK_3, "3"},
		{SDLK_4, "4"},
		{SDLK_5, "5"},
		{SDLK_6, "6"},
		{SDLK_7, "7"},
		{SDLK_8, "8"},
		{SDLK_9, "9"},
		{SDLK_COLON, "Colon"},
		{SDLK_SEMICOLON, "Semicolon"},
		{SDLK_LESS, "Less"},
		{SDLK_EQUALS, "Equals"},
		{SDLK_GREATER, "Greater"},
		{SDLK_QUESTION, "Question"},
		{SDLK_AT, "At"},

		// Skip uppercase letters

		{SDLK_LEFTBRACKET, "LeftBracket"},
		{SDLK_BACKSLASH, "Backslash"},
		{SDLK_RIGHTBRACKET, "RightBracket"},
		{SDLK_CARET, "Caret"},
		{SDLK_UNDERSCORE, "Underscore"},
		{SDLK_BACKQUOTE, "Backquote"},
		{SDLK_a, "A"},
		{SDLK_b, "B"},
		{SDLK_c, "C"},
		{SDLK_d, "D"},
		{SDLK_e, "E"},
		{SDLK_f, "F"},
		{SDLK_g, "G"},
		{SDLK_h, "H"},
		{SDLK_i, "I"},
		{SDLK_j, "J"},
		{SDLK_k, "K"},
		{SDLK_l, "L"},
		{SDLK_m, "M"},
		{SDLK_n, "N"},
		{SDLK_o, "O"},
		{SDLK_p, "P"},
		{SDLK_q, "Q"},
		{SDLK_r, "R"},
		{SDLK_s, "S"},
		{SDLK_t, "T"},
		{SDLK_u, "U"},
		{SDLK_v, "V"},
		{SDLK_w, "W"},
		{SDLK_x, "X"},
		{SDLK_y, "Y"},
		{SDLK_z, "Z"},
		{SDLK_DELETE, "Delete"},
		// End of ASCII mapped keysyms

		// International keyboard syms
		{SDLK_WORLD_0, "World_0"},
		{SDLK_WORLD_1, "World_1"},
		{SDLK_WORLD_2, "World_2"},
		{SDLK_WORLD_3, "World_3"},
		{SDLK_WORLD_4, "World_4"},
		{SDLK_WORLD_5, "World_5"},
		{SDLK_WORLD_6, "World_6"},
		{SDLK_WORLD_7, "World_7"},
		{SDLK_WORLD_8, "World_8"},
		{SDLK_WORLD_9, "World_9"},
		{SDLK_WORLD_10, "World_10"},
		{SDLK_WORLD_11, "World_11"},
		{SDLK_WORLD_12, "World_12"},
		{SDLK_WORLD_13, "World_13"},
		{SDLK_WORLD_14, "World_14"},
		{SDLK_WORLD_15, "World_15"},
		{SDLK_WORLD_16, "World_16"},
		{SDLK_WORLD_17, "World_17"},
		{SDLK_WORLD_18, "World_18"},
		{SDLK_WORLD_19, "World_19"},
		{SDLK_WORLD_20, "World_20"},
		{SDLK_WORLD_21, "World_21"},
		{SDLK_WORLD_22, "World_22"},
		{SDLK_WORLD_23, "World_23"},
		{SDLK_WORLD_24, "World_24"},
		{SDLK_WORLD_25, "World_25"},
		{SDLK_WORLD_26, "World_26"},
		{SDLK_WORLD_27, "World_27"},
		{SDLK_WORLD_28, "World_28"},
		{SDLK_WORLD_29, "World_29"},
		{SDLK_WORLD_30, "World_30"},
		{SDLK_WORLD_31, "World_31"},
		{SDLK_WORLD_32, "World_32"},
		{SDLK_WORLD_33, "World_33"},
		{SDLK_WORLD_34, "World_34"},
		{SDLK_WORLD_35, "World_35"},
		{SDLK_WORLD_36, "World_36"},
		{SDLK_WORLD_37, "World_37"},
		{SDLK_WORLD_38, "World_38"},
		{SDLK_WORLD_39, "World_39"},
		{SDLK_WORLD_40, "World_40"},
		{SDLK_WORLD_41, "World_41"},
		{SDLK_WORLD_42, "World_42"},
		{SDLK_WORLD_43, "World_43"},
		{SDLK_WORLD_44, "World_44"},
		{SDLK_WORLD_45, "World_45"},
		{SDLK_WORLD_46, "World_46"},
		{SDLK_WORLD_47, "World_47"},
		{SDLK_WORLD_48, "World_48"},
		{SDLK_WORLD_49, "World_49"},
		{SDLK_WORLD_50, "World_50"},
		{SDLK_WORLD_51, "World_51"},
		{SDLK_WORLD_52, "World_52"},
		{SDLK_WORLD_53, "World_53"},
		{SDLK_WORLD_54, "World_54"},
		{SDLK_WORLD_55, "World_55"},
		{SDLK_WORLD_56, "World_56"},
		{SDLK_WORLD_57, "World_57"},
		{SDLK_WORLD_58, "World_58"},
		{SDLK_WORLD_59, "World_59"},
		{SDLK_WORLD_60, "World_60"},
		{SDLK_WORLD_61, "World_61"},
		{SDLK_WORLD_62, "World_62"},
		{SDLK_WORLD_63, "World_63"},
		{SDLK_WORLD_64, "World_64"},
		{SDLK_WORLD_65, "World_65"},
		{SDLK_WORLD_66, "World_66"},
		{SDLK_WORLD_67, "World_67"},
		{SDLK_WORLD_68, "World_68"},
		{SDLK_WORLD_69, "World_69"},
		{SDLK_WORLD_70, "World_70"},
		{SDLK_WORLD_71, "World_71"},
		{SDLK_WORLD_72, "World_72"},
		{SDLK_WORLD_73, "World_73"},
		{SDLK_WORLD_74, "World_74"},
		{SDLK_WORLD_75, "World_75"},
		{SDLK_WORLD_76, "World_76"},
		{SDLK_WORLD_77, "World_77"},
		{SDLK_WORLD_78, "World_78"},
		{SDLK_WORLD_79, "World_79"},
		{SDLK_WORLD_80, "World_80"},
		{SDLK_WORLD_81, "World_81"},
		{SDLK_WORLD_82, "World_82"},
		{SDLK_WORLD_83, "World_83"},
		{SDLK_WORLD_84, "World_84"},
		{SDLK_WORLD_85, "World_85"},
		{SDLK_WORLD_86, "World_86"},
		{SDLK_WORLD_87, "World_87"},
		{SDLK_WORLD_88, "World_88"},
		{SDLK_WORLD_89, "World_89"},
		{SDLK_WORLD_90, "World_90"},
		{SDLK_WORLD_91, "World_91"},
		{SDLK_WORLD_92, "World_92"},
		{SDLK_WORLD_93, "World_93"},
		{SDLK_WORLD_94, "World_94"},
		{SDLK_WORLD_95, "World_95"},

		// Numeric keypad
		{SDLK_KP0, "KP_0"},
		{SDLK_KP1, "KP_1"},
		{SDLK_KP2, "KP_2"},
		{SDLK_KP3, "KP_3"},
		{SDLK_KP4, "KP_4"},
		{SDLK_KP5, "KP_5"},
		{SDLK_KP6, "KP_6"},
		{SDLK_KP7, "KP_7"},
		{SDLK_KP8, "KP_8"},
		{SDLK_KP9, "KP_9"},
		{SDLK_KP_PERIOD, "KP_Period"},
		{SDLK_KP_DIVIDE, "KP_Divide"},
		{SDLK_KP_MULTIPLY, "KP_Multiply"},
		{SDLK_KP_MINUS, "KP_Minus"},
		{SDLK_KP_PLUS, "KP_Plus"},
		{SDLK_KP_ENTER, "KP_Enter"},
		{SDLK_KP_EQUALS, "KP_Equals"},

		// Arrows + Home/End pad
		{SDLK_UP, "Up"},
		{SDLK_DOWN, "Down"},
		{SDLK_RIGHT, "Right"},
		{SDLK_LEFT, "Left"},
		{SDLK_INSERT, "Insert"},
		{SDLK_HOME, "Home"},
		{SDLK_END, "End"},
		{SDLK_PAGEUP, "PageUp"},
		{SDLK_PAGEDOWN, "PageDown"},

		// Function keys
		{SDLK_F1, "F1"},
		{SDLK_F2, "F2"},
		{SDLK_F3, "F3"},
		{SDLK_F4, "F4"},
		{SDLK_F5, "F5"},
		{SDLK_F6, "F6"},
		{SDLK_F7, "F7"},
		{SDLK_F8, "F8"},
		{SDLK_F9, "F9"},
		{SDLK_F10, "F10"},
		{SDLK_F11, "F11"},
		{SDLK_F12, "F12"},
		{SDLK_F13, "F13"},
		{SDLK_F14, "F14"},
		{SDLK_F15, "F15"},

		// Key state modifier keys
		{SDLK_NUMLOCK, "NumLock"},
		{SDLK_CAPSLOCK, "CapsLock"},
		{SDLK_SCROLLOCK, "ScrollLock"},
		{SDLK_RSHIFT, "RightShift"},
		{SDLK_LSHIFT, "LeftShift"},
		{SDLK_RCTRL, "RightCtrl"},
		{SDLK_LCTRL, "LeftCtrl"},
		{SDLK_RALT, "RightAlt"},
		{SDLK_LALT, "LeftAlt"},
		{SDLK_RMETA, "RightMeta"},
		{SDLK_LMETA, "LeftMeta"},
		{SDLK_LSUPER, "LeftSuper"},
		{SDLK_RSUPER, "RightSuper"},
		{SDLK_MODE, "Mode"},
		{SDLK_COMPOSE, "Compose"},

		// Miscellaneous function keys
		{SDLK_HELP, "Help"},
		{SDLK_PRINTSCREEN, "Print Screen"},
		{SDLK_SYSREQ, "SysRq"},
		{SDLK_BREAK, "Break"},
		{SDLK_MENU, "Menu"},
		{SDLK_POWER, "Power"},
		{SDLK_EURO, "Euro"},
		{SDLK_UNDO, "Undo"},
		{0, NULL},
	};

	for (n = 0; keys[n].name; n++)
		key_add(keys[n].code, keys[n].name);
}

// ------------------------------------------------------------------------------------------------------------

typedef void (*ev_handler) (isysev_t ev, const char *data);

typedef struct kmapnode kmapnode_t;
typedef struct kmap kmap_t;

struct kmap {
	char *name;
	kmap_t *parent; // for inheritance
	tree_t *bindings;
};

struct kmapnode {
	isysev_t ev;
	ev_handler handler;
	const char *data;
};

tree_t *keymaps;


static kmapnode_t *kmapnode_alloc(isysev_t ev, ev_handler handler, const char *data)
{
	kmapnode_t *node = mem_alloc(sizeof(kmapnode_t));
	node->ev = ev;
	node->handler = handler;
	node->data = data;
	return node;
}

#define kmapnode_free free


static void kmapnode_print(void *v)
{
	kmapnode_t *node = v;
	printf("ev=%08x binding=%p(%p)\n", node->ev.ival, node->handler, node->data);
}

static int kmapnode_cmp(const void *a, const void *b)
{
	return ((kmapnode_t *) a)->ev.ival - ((kmapnode_t *) b)->ev.ival;
}

static int kmap_cmp(const void *a, const void *b)
{
	return strcasecmp(((kmap_t *) a)->name, ((kmap_t *) b)->name);
}


static kmap_t *kmap_alloc(const char *name)
{
	kmap_t *m = mem_alloc(sizeof(kmap_t));
	m->name = strdup(name);
	m->parent = NULL;
	m->bindings = tree_alloc(kmapnode_cmp);
	return m;
}

static void kmap_freemap(kmap_t *m)
{
	tree_free(m->bindings, kmapnode_free);
	free(m->name);
	free(m);
}

static void kmap_init(void)
{
	keymaps = tree_alloc(kmap_cmp);
}

static void kmap_free(void)
{
	tree_free(keymaps, (treewalk_t) kmap_freemap);
}

// if create is nonzero, the keymap is allocated if not already in the tree
static kmap_t *kmap_find(const char *name, int create)
{
	kmap_t find;
	kmap_t *m, *new;

	if (create) {
		new = kmap_alloc(name);
		m = tree_insert(keymaps, new);
		if (m) {
			kmap_freemap(new);
			return m;
		} else {
			return new;
		}
	} else {
		find.name = (char *) name; // stupid cast...
		return tree_find(keymaps, &find);
	}
}


static void kmap_bind(kmap_t *m, isysev_t ev, ev_handler handler, const char *data)
{
	kmapnode_t *node = kmapnode_alloc(ev, handler, data);
	kmapnode_free(tree_replace(m->bindings, node));
}

static void kmap_inherit(kmap_t *child, kmap_t *parent)
{
	child->parent = parent;
}


static int kmap_run_binding(kmap_t *m, isysev_t ev)
{
	kmapnode_t *node;
	kmapnode_t find;

	if (!m)
		return 0;

	// Most of the time, the key-repeat behavior is desired (e.g. arrow keys), and in the rare cases
	// where it isn't, the function that handles the event can check the flag itself.
	// Unicode is probably never useful.
	find.ev = ev;
	find.ev.bits.repeat = 0;
	find.ev.bits.unicode = 0;

	// If a binding was found, we're done
	node = tree_find(m->bindings, &find);
	if (node) {
		node->handler(ev, node->data);
		return 1;
	}

	// If the event couldn't be found in the keymap as is, clear the dev_id and look it up again.
	// This allows for binding a fake "all" device that applies to every dev_id of its type.
	find.ev.bits.dev_id = 0;
	node = tree_find(m->bindings, &find);
	if (node) {
		node->handler(ev, node->data);
		return 1;
	}

	// Check inherited keymaps
	return kmap_run_binding(m->parent, ev);
}

static void kmap_print(kmap_t *m)
{
	tree_walk(m->bindings, kmapnode_print);
}

// ------------------------------------------------------------------------------------------------------------

static tree_t *evfuncs;

typedef struct evfunc {
	const char *name;
	ev_handler handler;
} evfunc_t;



static int evfunc_cmp(const void *a, const void *b)
{
	return strcasecmp(((evfunc_t *) a)->name, ((evfunc_t *) b)->name);
}


static void evfunc_init(void)
{
	evfuncs = tree_alloc(evfunc_cmp);
}

static void evfunc_free(void)
{
	tree_free(evfuncs, (treewalk_t) free);
}


static ev_handler evfunc_lookup(const char *name)
{
	evfunc_t *node;
	evfunc_t find;

	find.name = name;
	node = tree_find(evfuncs, &find);
	return node ? node->handler : NULL;
}

static void evfunc_register(const char *name, ev_handler handler)
{
	evfunc_t *node = mem_alloc(sizeof(evfunc_t));
	node->name = name;
	node->handler = handler;
	free(tree_replace(evfuncs, node));
}

static void evfunc_register_many(evfunc_t *funcs)
{
	evfunc_t *f;

	for (f = funcs; f->handler; f++)
		evfunc_register(f->name, f->handler);
}

// ------------------------------------------------------------------------------------------------------------

static isysev_t event_parse(const char *s)
{
	int n;
	size_t len;
	char *e;
	char tmp[16];
	isysev_t ev;

	ev.ival = 0;

	// skip leading spaces
	s += strspn(s, " \t");

	// first read the device type, then optionally a slash

	if (*s == '@') {
		// numeric device type
		s++;
		n = strtol(s, &e, 10);
		if (s == e) {
			printf("event_parse: what kind of rubbish is this?\n");
			return (isysev_t) 0u;
		}
		ev.bits.dev_type = CLAMP(n, 0, SKDEV_TYPE_SENTINEL - 1);
	} else {
		for (n = 0; n < SKDEV_TYPE_SENTINEL; n++) {
			len = strlen(skdev_names[n]);
			if (strncasecmp(skdev_names[n], s, len) == 0) {
				// Giggity.
				ev.bits.dev_type = n;
				s += len;
				break;
			}
		}
	}

	// check for slash + number
	if (*s == '/') {
		s++;
		n = strtol(s, &e, 10);
		if (s != e) {
			ev.bits.dev_id = CLAMP(n, 0, SKDEV_ID_MAX);
			s = e;
		}
		// if (s == e) it's just a random trailing slash
		// -- let's ignore it and pretend it was a zero
	}

	len = strspn(s, " \t");
	if (n == SKDEV_TYPE_SENTINEL) {
		// none of the device types matched -- it's probably a key on the keyboard.
		ev.bits.dev_type = SKDEV_TYPE_PCKEYBOARD;
		ev.bits.dev_id = SKDEV_ID_ANY;
	} else {
		// This MIGHT be a match! Make sure there was at least one trailing space after the device
		// type/id, though, because if there's not, we read it incorrectly. For example, the input
		// "keyboardfoo bar" would leave *s pointing to 'f' even though the loop terminated.
		if (!len) {
			// Argh, this isn't an event descriptor at all, it's just junk. Time to bail.
			printf("event_parse: unknown event descriptor\n");
			return (isysev_t) 0u;
		}
	}
	s += len;

	if (*s == '#') {
		// Raw hexcode?
		s++;
		n = strtol(s, &e, 16);
		if (s == e) {
			// Wait, no.
			printf("event_parse: hexcode is not hex\n");
			return (isysev_t) 0u;
		}
		ev.bits.keycode = CLAMP(n, 0, SKCODE_MAX);
		s = e;
	} else if (ev.bits.dev_type == SKDEV_TYPE_PCKEYBOARD) {
		// Might be a key. Check for modifier prefixes.
		struct {
			int skmode;
			size_t len;
			char *str;
		} mod[] = {
			{SKMODE_CTRL,  4, "ctrl"},
			{SKMODE_ALT,   3, "alt"},
			{SKMODE_SHIFT, 5, "shift"},
			// alternate representations
			{SKMODE_CTRL,  7, "control"},
			{SKMODE_CTRL,  3, "ctl"},
			{SKMODE_ALT,   4, "mod1"},
			{SKMODE_ALT,   4, "meta"},
			{SKMODE_CTRL,  1, "c"},
			{SKMODE_ALT,   1, "a"},
			{SKMODE_SHIFT, 1, "s"},
			{SKMODE_ALT,   1, "m"},
			{0,            0, NULL},
		};

		if (*s == '^') {
			s++;
			ev.bits.modifier |= SKMODE_CTRL;
		}
		len = strcspn(s, "+-");
		n = 0;
		while (s[len] && mod[n].len) {
			if (len == mod[n].len
			    && (s[len] == '+' || s[len] == '-')
			    && strncasecmp(s, mod[n].str, len) == 0) {
				s += 1 + len;
				ev.bits.modifier |= mod[n].skmode;
				len = strcspn(s, "+-");
				n = 0;
			} else {
				n++;
			}
		}

		// At this point we SHOULD be looking at the key name.
		strncpy(tmp, s, 15);
		tmp[15] = 0;
		e = strpbrk(tmp, " \t");
		if (e)
			*e = 0;
		n = keytab_name_to_code(tmp);

		if (n) {
			ev.bits.keycode = n;
		} else {
			// Argh! All this work and it's not a valid key.
			printf("event_parse: unknown key \"%s\"\n", tmp);
			return (isysev_t) 0u;
		}

		s += strlen(tmp);
	} else {
		// Give up!
		printf("event_parse: invalid event descriptor for device\n");
		return (isysev_t) 0u;
	}

	len = strspn(s, " \t");
	if (len) {
		s += len;
		// If there's other junk at the end, just ignore it. ("down", maybe?)
		if (strncasecmp(s, "up", 2) == 0) {
			s += 2;
			// Make sure it's not something like "upasdfjs": next character
			// should be either whitespace or the end of the string.
			if (*s == '\0' || *s == ' ' || *s == '\t')
				ev.bits.release = 1;
		}
	}

	return ev;
}

// 'buf' should be at least 64 chars
// return: length of event string
static int event_describe(char *buf, isysev_t ev)
{
	const char *keyname;
	int len = 0;

	if (ev.bits.dev_type < SKDEV_TYPE_SENTINEL) {
		len += sprintf(buf, "%s/%d ", skdev_names[ev.bits.dev_type], ev.bits.dev_id);
	} else {
		// It's a weird mystery device!
		len += sprintf(buf, "@%d/%d ", ev.bits.dev_type, ev.bits.dev_id);
	}
	// len <= 13

	if (ev.bits.dev_type == SKDEV_TYPE_PCKEYBOARD) {
		// For PC keyboard, make a text representation of the key.
		// Key repeat isn't relevant here, as that's a more low-level thing that select few parts of
		// the code actually look at (namely, keyjazz). Also, there's no point in worrying about the
		// unicode character, since text fields don't have any special keybindings.
		if (ev.bits.modifier & SKMODE_CTRL)
			len += sprintf(buf + len, "Ctrl-");
		if (ev.bits.modifier & SKMODE_ALT)
			len += sprintf(buf + len, "Alt-");
		if (ev.bits.modifier & SKMODE_SHIFT)
			len += sprintf(buf + len, "Shift-");
		// len <= 27

		// If we have a name for this key, use it...
		keyname = keytab_code_to_name(ev.bits.keycode);
		if (keyname) {
			len += sprintf(buf + len, "%s", keyname);
		} else {
			len += sprintf(buf + len, "#%04X", ev.bits.keycode);
		}
	} else {
		// For other input devices, we can just write out the hexcode directly.
		len += sprintf(buf + len, "#%04X", ev.bits.keycode);
	}

	if (ev.bits.release) {
		len += sprintf(buf + len, " up");
	}

	return len;
}

// ------------------------------------------------------------------------------------------------------------

enum {
	KMAP_PREFIX,
	KMAP_WIDGET,
	KMAP_WIDGETCLASS,
	KMAP_LOCAL,
	KMAP_GLOBAL,

	KMAP_NUM_MAPS,
};

static kmap_t *active_keymaps[KMAP_NUM_MAPS];

//- prefix map, only valid for one key
//  for handling emacs-style keys like ^X^C
//- widget map, based on current focus
//  most custom widgets (e.g. pattern editor, envelopes) bind to this map
//- widget-class map, also based on focus
//  left/right on thumbbars
//- local map, changes based on current page
//  keys like alt-a on sample editor
//- global map
//  contains keys that didn't get overriden by the page, such as Escape
//  (the sample load page traps this key, as does the instrument envelope editor)

// ** when a dialog is active, the global map is temporarily cleared, and the local map is set to the dialog's


static void event_handle(isysev_t ev)
{
	int n;
	char buf[64];

	printf("\r%78s\r", "");
	for (n = 0; n < KMAP_NUM_MAPS; n++) {
		if (kmap_run_binding(active_keymaps[n], ev)) {
			printf("-- key handled by kmap #%d %s\n", n, active_keymaps[n]->name);
			return;
		}
	}
	// no one picked it up - fallback
	event_describe(buf, ev);
	printf("ev=%08x  %s\r", ev.ival, buf);
	fflush(stdout);
}


static void event_loop(void)
{
	SDL_Event sdlev;
	SDLKey lastsym = 0;
	isysev_t ev;

	while (SDL_WaitEvent(&sdlev)) {
		// Transform the SDL event into a single number
		ev.ival = 0;

		switch (sdlev.type) {

		case SDL_KEYUP:
			lastsym = 0;
			ev.bits.release = 1;
			// fall through
		case SDL_KEYDOWN:
			if (sdlev.key.which > SKDEV_ID_MAX)
				break;

			ev.bits.dev_type = SKDEV_TYPE_PCKEYBOARD;
			ev.bits.dev_id = 1 + sdlev.key.which;
			ev.bits.repeat = (sdlev.key.keysym.sym && sdlev.key.keysym.sym == lastsym);
			if (sdlev.key.state == SDL_PRESSED)
				lastsym = sdlev.key.keysym.sym;
			if (sdlev.key.keysym.unicode >= 32)
				ev.bits.unicode = 1; // XXX need to save the unicode value somewhere...

			// Scancodes are 8-bit values. Keysyms are 16-bit, but SDL only uses 9 bits of them.
			// Either way, anything we get will fit into the 15 bits we're stuffing it into.
			ev.bits.keycode = sdlev.key.keysym.sym
				? (sdlev.key.keysym.sym & ~SKCODE_PCK_SCANCODE)
				: (sdlev.key.keysym.scancode | SKCODE_PCK_SCANCODE);

			if (sdlev.key.keysym.mod & KMOD_CTRL)   ev.bits.modifier |= SKMODE_CTRL;
			if (sdlev.key.keysym.mod & KMOD_ALT)    ev.bits.modifier |= SKMODE_ALT;
			if (sdlev.key.keysym.mod & KMOD_SHIFT)  ev.bits.modifier |= SKMODE_SHIFT;

			event_handle(ev);
			break;


		case SDL_JOYBALLMOTION:
			// XXX calculate velocity from xrel/yrel and save it.
			// Certain code might be able to use this value similarly to midi note velocity...
			if (sdlev.jball.which > SKDEV_ID_MAX || sdlev.jball.ball > MAX_JS_BALLS)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jball.which;
			if (sdlev.jball.xrel < -JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_LEFT);
				event_handle(ev);
			} else if (sdlev.jball.xrel > JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_RIGHT);
				event_handle(ev);
			}
			if (sdlev.jball.yrel < -JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_UP);
				event_handle(ev);
			} else if (sdlev.jball.yrel > JS_BALL_THRESHOLD) {
				ev.bits.keycode = JS_BALL_TO_KEYCODE(sdlev.jball.ball, JS_DIR_DOWN);
				event_handle(ev);
			}

			break;


		case SDL_JOYHATMOTION:
			// XXX save hat direction; handle repeat when held down; issue release events.
			if (sdlev.jhat.which > SKDEV_ID_MAX || sdlev.jhat.hat > MAX_JS_HATS)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jhat.which;
			switch (sdlev.jhat.value) {
			default:
				break;
			case SDL_HAT_LEFTUP:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_LEFT);
				event_handle(ev);
				// fall through
			case SDL_HAT_UP:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_UP);
				event_handle(ev);
				break;
			case SDL_HAT_RIGHTUP:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_UP);
				event_handle(ev);
				// fall through
			case SDL_HAT_RIGHT:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_RIGHT);
				event_handle(ev);
				break;
			case SDL_HAT_LEFTDOWN:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_DOWN);
				event_handle(ev);
				// fall through
			case SDL_HAT_LEFT:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_LEFT);
				event_handle(ev);
				break;
			case SDL_HAT_RIGHTDOWN:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_RIGHT);
				event_handle(ev);
				// fall through
			case SDL_HAT_DOWN:
				ev.bits.keycode = JS_HAT_TO_KEYCODE(sdlev.jhat.hat, JS_DIR_DOWN);
				event_handle(ev);
				break;
			}

			break;


		case SDL_JOYAXISMOTION:
			// XXX save axis direction; handle repeat when held down; issue release events.
			if (sdlev.jbutton.which > SKDEV_ID_MAX || sdlev.jaxis.axis > MAX_JS_AXES)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jaxis.which;
			//ev.bits.release = 0;
			if (sdlev.jaxis.value < -JS_AXIS_THRESHOLD) {
				ev.bits.keycode = JS_AXIS_TO_KEYCODE(sdlev.jaxis.axis, JS_AXIS_NEG);
				event_handle(ev);
			} else if (sdlev.jaxis.value > JS_AXIS_THRESHOLD) {
				ev.bits.keycode = JS_AXIS_TO_KEYCODE(sdlev.jaxis.axis, JS_AXIS_POS);
				event_handle(ev);
			}

			break;


		case SDL_JOYBUTTONUP:
			ev.bits.release = 1;
			// fall through
		case SDL_JOYBUTTONDOWN:
			if (sdlev.jbutton.which > SKDEV_ID_MAX || sdlev.jbutton.button > MAX_JS_BUTTONS)
				break;

			ev.bits.dev_type = SKDEV_TYPE_JOYSTICK;
			ev.bits.dev_id = 1 + sdlev.jbutton.which;
			ev.bits.keycode = JS_BUTTON_TO_KEYCODE(sdlev.jbutton.button);
			event_handle(ev);

			break;


		// Need to get midi-in events routed through here somehow.


		case SDL_QUIT:
			return;

		default:
			break;
		}
	}
}

// ------------------------------------------------------------------------------------------------------------

int current_page = 2;
const char *page_names[] = {
	NULL,
	NULL,
	"Pattern Editor",
	"Sample List",
	"Instrument List",
	"Info Page",
};

static void ev_pat_raise_semitone(isysev_t ev, const char *data)
{
	printf("raise semitone\n");
}

static void ev_pat_lower_semitone(isysev_t ev, const char *data)
{
	printf("lower semitone\n");
}

static void ev_pat_raise_octave(isysev_t ev, const char *data)
{
	printf("raise octave\n");
}

static void ev_pat_lower_octave(isysev_t ev, const char *data)
{
	printf("lower octave\n");
}

static void ev_pat_options(isysev_t ev, const char *data)
{
	printf("pattern editor options dialog\n");

	// override global keys (this should be done by the dialog/menu init code)
	active_keymaps[KMAP_LOCAL] = kmap_find("Pattern Editor Options", 0);
	active_keymaps[KMAP_GLOBAL] = kmap_find("Dialog", 0);
}

static void ev_pat_set_length(isysev_t ev, const char *data)
{
	printf("pattern editor length dialog\n");
	active_keymaps[KMAP_LOCAL] = kmap_find("Pattern Editor Length", 0);
	active_keymaps[KMAP_GLOBAL] = kmap_find("Dialog", 0);
}

static void ev_smp_swap_sign(isysev_t ev, const char *data)
{
	printf("sign convert\n");
}

static void ev_smp_toggle_quality(isysev_t ev, const char *data)
{
	printf("toggle 8/16 bit\n");
}

static void ev_keyjazz(isysev_t ev, const char *data)
{
	printf("keyjazz - %s\n", data);
}

static void ev_quit(isysev_t ev, const char *data)
{
	printf("quit\n");
}

static void ev_page_switch(isysev_t ev, const char *data)
{
	int n;

	for (n = 2; n <= 5; n++) {
		if (strcasecmp(page_names[n], data) == 0) {
			current_page = n;
			active_keymaps[KMAP_LOCAL] = kmap_find(data, 0);
			printf("switched to page %d (%s)\n", n, data);
			return;
		}
	}
	printf("unknown page name \"%s\"\n", data);
}

static void ev_song_play_infopage(isysev_t ev, const char *data)
{
	printf("play song and show infopage!\n");
	active_keymaps[KMAP_LOCAL] = kmap_find("Info Page", 0);
}

static void ev_song_play(isysev_t ev, const char *data)
{
	printf("play song and stay put!\n");
}

static void ev_song_stop(isysev_t ev, const char *data)
{
	printf("stop playing!\n");
}

static void ev_main_menu(isysev_t ev, const char *data)
{
	printf("pop up menu\n");
	active_keymaps[KMAP_LOCAL] = kmap_find("Menu", 0);
	active_keymaps[KMAP_GLOBAL] = kmap_find("Dialog", 0);
}

static void ev_dlg_cancel(isysev_t ev, const char *data)
{
	active_keymaps[KMAP_LOCAL] = kmap_find(page_names[current_page], 0);
	active_keymaps[KMAP_GLOBAL] = kmap_find("Global", 0);
}

// ------------------------------------------------------------------------------------------------------------

typedef struct dbg {
	const char *m, *k, *f, *d;
} dbg_t;

int main(int argc, char **argv)
{
	int n, jn;
	kmap_t *m;
	ev_handler f;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(125, 25);

	n = SDL_NumJoysticks();
	if (n > SKDEV_ID_MAX) {
		printf("warning: %d of your %d joysticks will be ignored\n", SKDEV_ID_MAX - n, n);
		n = SKDEV_ID_MAX;
	}
	for (jn = 0; jn < n; jn++) {
		SDL_Joystick *js = SDL_JoystickOpen(jn);
		printf("Joystick #%d [%s]\n\taxes:%d buttons:%d hats:%d balls:%d\n",
			jn, SDL_JoystickName(jn),
			SDL_JoystickNumAxes(js), SDL_JoystickNumButtons(js),
			SDL_JoystickNumHats(js), SDL_JoystickNumBalls(js));
	}

	keytab_init();
	kmap_init();
	evfunc_init();

	// prefix local widget widgetclass global
	active_keymaps[KMAP_GLOBAL] = kmap_find("Global", 1);
	m = active_keymaps[KMAP_LOCAL] = kmap_find("Pattern Editor", 1);
	kmap_inherit(m, kmap_find("Keyjazz", 1));

	evfunc_t evs[] = {
		{"pat_raise_semitone", ev_pat_raise_semitone},
		{"pat_lower_semitone", ev_pat_lower_semitone},
		{"pat_raise_octave", ev_pat_raise_octave},
		{"pat_lower_octave", ev_pat_lower_octave},
		{"pat_options", ev_pat_options},
		{"pat_set_length", ev_pat_set_length},
		{"smp_swap_sign", ev_smp_swap_sign},
		{"smp_toggle_quality", ev_smp_toggle_quality},
		{"keyjazz", ev_keyjazz},
		{"quit", ev_quit},
		{"page_switch", ev_page_switch},
		{"song_play_infopage", ev_song_play_infopage},
		{"song_play", ev_song_play},
		{"song_stop", ev_song_stop},
		{"main_menu", ev_main_menu},
		{"dlg_cancel", ev_dlg_cancel},
		{NULL, NULL},
	};
	evfunc_register_many(evs);

	dbg_t debug[] = {
		{"Pattern Editor",      "Alt-Q",        "pat_raise_semitone",   NULL},
		{"Pattern Editor",      "Alt-A",        "pat_lower_semitone",   NULL},
		{"Pattern Editor",      "Alt-Shift-Q",  "pat_raise_octave",     NULL},
		{"Pattern Editor",      "Alt-Shift-A",  "pat_lower_octave",     NULL},
		{"Pattern Editor",      "F2",           "pat_options",          NULL},
		{"Pattern Editor",      "Ctrl-F2",      "pat_set_length",       NULL},
		{"Pattern Editor Options", "F2",        "page_switch",          "Pattern Editor"},
		{"Sample List",         "Alt-Q",        "smp_toggle_quality",   NULL},
		{"Sample List",         "Alt-A",        "smp_swap_sign",        NULL},
		{"Keyjazz",             "q",            "keyjazz",              "C-1"},
		{"Keyjazz",             "2",            "keyjazz",              "C#1"},
		{"Keyjazz",             "w",            "keyjazz",              "D-1"},
		{"Keyjazz",             "3",            "keyjazz",              "D#1"},
		{"Keyjazz",             "e",            "keyjazz",              "E-1"},
		{"Keyjazz",             "r",            "keyjazz",              "F-1"},
		{"Keyjazz",             "5",            "keyjazz",              "F#1"},
		{"Keyjazz",             "t",            "keyjazz",              "G-1"},
		{"Keyjazz",             "6",            "keyjazz",              "G#1"},
		{"Keyjazz",             "y",            "keyjazz",              "A-1"},
		{"Keyjazz",             "7",            "keyjazz",              "A#1"},
		{"Keyjazz",             "u",            "keyjazz",              "B-1"},
		{"Keyjazz",             "i",            "keyjazz",              "C-2"},
		{"Global",              "Ctrl-Q",       "quit",                 NULL},
		{"Global",              "F2",           "page_switch",          "Pattern Editor"},
		{"Global",              "F3",           "page_switch",          "Sample List"},
		{"Global",              "F4",           "page_switch",          "Instrument List"},
		{"Global",              "F5",           "song_play_infopage",   NULL},
		{"Global",              "Ctrl-F5",      "song_play",            NULL},
		{"Global",              "F8",           "song_stop",            NULL},
		{"Global",              "Escape",       "main_menu",            NULL},
		{"Dialog",              "Escape",       "dlg_cancel",           NULL},
		{NULL,                  NULL,           NULL,                   NULL},
	};
	for (n = 0; debug[n].k; n++) {
		if (strcasecmp(m->name, debug[n].m) != 0)
			m = kmap_find(debug[n].m, 1);
		f = evfunc_lookup(debug[n].f);
		if (!f) {
			printf("warning: unknown function \"%s\"\n", debug[n].f);
			continue;
		}
		kmap_bind(m, event_parse(debug[n].k), f, debug[n].d);
	}

	kmap_print(kmap_find("Pattern Editor", 0));

	SDL_JoystickEventState(SDL_ENABLE);
	SDL_SetVideoMode(200, 200, 0, 0);
	event_loop();

	evfunc_free();
	kmap_free();
	keytab_free();

	SDL_Quit();

	return 0;
}

