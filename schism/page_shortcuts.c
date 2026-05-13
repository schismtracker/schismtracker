#include "headers.h"

#include "config.h"
#include "it.h"
#include "keyboard.h"
#include "page.h"
#include "shortcuts.h"
#include "widget.h"
#include "vgamem.h"

static struct widget widgets_shortcuts[5];
static int shortcuts_selected = 0;
static int shortcuts_capturing = 0;
static int shortcuts_top = 0;
#define SHORTCUTS_ROWS_VISIBLE 22

static int shortcuts_group_break_before(int idx)
{
	/* Keep display grouped in the same order as shortcuts.c */
	switch (idx) {
	case 4:   /* F-key page jumps */
	case 22:  /* app commands */
	case 38:  /* playback params */
	case 46:  /* pattern editor */
		return 1;
	default:
		return 0;
	}
}

int shortcuts_is_capture_mode(void)
{
	return shortcuts_capturing;
}

static void shortcuts_action_rebind(void)
{
	shortcuts_capturing = 1;
	status_text_flash("Press new key combination");
	status.flags |= NEED_UPDATE;
}

static void shortcuts_action_defaults(void)
{
	shortcuts_reset_defaults();
	status_text_flash("Shortcuts reset to defaults (press Save to persist)");
	status.flags |= NEED_UPDATE;
}

static void shortcuts_action_save(void)
{
	cfg_save();
	status_text_flash("Shortcut settings saved");
}

static void shortcuts_action_back(void)
{
	set_page(PAGE_CONFIG);
}

static int shortcuts_page_handle_key(struct key_event *k)
{
	int count = shortcuts_count();

	if (shortcuts_capturing) {
		if (k->state == KEY_RELEASE) {
			return 1;
		}
		if (k->sym == SCHISM_KEYSYM_ESCAPE) {
			shortcuts_capturing = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		}
		if (k->sym == SCHISM_KEYSYM_RETURN || k->sym == SCHISM_KEYSYM_KP_ENTER) {
			status_text_flash("Enter cannot be assigned");
			return 1;
		}
		if (k->sym == SCHISM_KEYSYM_LCTRL || k->sym == SCHISM_KEYSYM_RCTRL
		 || k->sym == SCHISM_KEYSYM_LALT || k->sym == SCHISM_KEYSYM_RALT
		 || k->sym == SCHISM_KEYSYM_LSHIFT || k->sym == SCHISM_KEYSYM_RSHIFT) {
			return 1;
		}
		if (shortcuts_set_binding(shortcuts_selected, k->sym, k->mod)) {
			shortcuts_capturing = 0;
			status_text_flash("Shortcut updated");
			status.flags |= NEED_UPDATE;
		} else {
			int conflict_idx = shortcuts_find_conflict(shortcuts_selected, k->sym, k->mod);
			if (conflict_idx >= 0) {
				status_text_flash("Conflict: %s", shortcuts_action_name(conflict_idx));
			} else {
				status_text_flash("Shortcut cannot be assigned");
			}
		}
		return 1;
	}

	if ((k->mod & SCHISM_KEYMOD_CTRL) && k->mouse == MOUSE_NONE) {
		status_text_flash("Use the on-screen buttons");
		return 1;
	}
	if (k->state == KEY_RELEASE) {
		return 0;
	}
	switch (k->sym) {
	case SCHISM_KEYSYM_UP:
		shortcuts_selected = CLAMP(shortcuts_selected - 1, 0, count - 1);
		if (shortcuts_selected < shortcuts_top)
			shortcuts_top = shortcuts_selected;
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_DOWN:
		shortcuts_selected = CLAMP(shortcuts_selected + 1, 0, count - 1);
		if (shortcuts_selected >= shortcuts_top + SHORTCUTS_ROWS_VISIBLE)
			shortcuts_top = shortcuts_selected - SHORTCUTS_ROWS_VISIBLE + 1;
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_PAGEUP:
		shortcuts_selected = CLAMP(shortcuts_selected - SHORTCUTS_ROWS_VISIBLE, 0, count - 1);
		shortcuts_top = CLAMP(shortcuts_top - SHORTCUTS_ROWS_VISIBLE, 0, count - SHORTCUTS_ROWS_VISIBLE);
		if (shortcuts_selected < shortcuts_top)
			shortcuts_top = shortcuts_selected;
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_PAGEDOWN:
		shortcuts_selected = CLAMP(shortcuts_selected + SHORTCUTS_ROWS_VISIBLE, 0, count - 1);
		shortcuts_top = CLAMP(shortcuts_top + SHORTCUTS_ROWS_VISIBLE, 0, count - SHORTCUTS_ROWS_VISIBLE);
		if (shortcuts_selected >= shortcuts_top + SHORTCUTS_ROWS_VISIBLE)
			shortcuts_top = shortcuts_selected - SHORTCUTS_ROWS_VISIBLE + 1;
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_RETURN:
	case SCHISM_KEYSYM_KP_ENTER:
		if (*selected_widget == 0) {
			shortcuts_action_rebind();
			return 1;
		}
		break;
	case SCHISM_KEYSYM_ESCAPE:
		shortcuts_action_back();
		return 1;
	default:
		break;
	}

	return 0;
}

static void shortcuts_page_draw(void)
{
	int i;
	int n;
	char binding[64];
	char posbuf[32];
	int count = shortcuts_count();
	int max_top = (count > SHORTCUTS_ROWS_VISIBLE) ? (count - SHORTCUTS_ROWS_VISIBLE) : 0;
	int top = CLAMP(shortcuts_top, 0, max_top);
	int end = top;

	draw_text("Global Shortcut Settings", 2, 13, 0, 3);
	draw_text("Use action buttons, then pick shortcut (PgUp/PgDn scroll)", 2, 14, 0, 2);
	draw_box(1, 15, 78, 45, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_fill_chars(2, 20, 77, 41, DEFAULT_FG, 0);

	for (i = top, n = 0; i < count && n < SHORTCUTS_ROWS_VISIBLE; i++) {
		int sym = 0;
		int mod = 0;
		int fg = (i == shortcuts_selected) ? 3 : 1;
		int bg = (i == shortcuts_selected) ? 1 : 0;

		if (i > top && shortcuts_group_break_before(i) && n < SHORTCUTS_ROWS_VISIBLE) {
			n++;
		}
		if (n >= SHORTCUTS_ROWS_VISIBLE) {
			break;
		}

		shortcuts_get_binding(i, &sym, &mod);
		shortcuts_format_binding(sym, mod, binding, sizeof(binding));

		draw_text(shortcuts_action_name(i), 3, 20 + n, bg, fg);
		draw_text(binding, 44, 20 + n, bg, fg);
		n++;
		end = i + 1;
	}
	if (count > SHORTCUTS_ROWS_VISIBLE) {
		snprintf(posbuf, sizeof(posbuf), "%d-%d/%d", top + 1, MAX(top + 1, end), count);
		draw_text(posbuf, 60, 42, 0, 2);
	}

	if (shortcuts_capturing) {
		draw_text("Waiting for key input...", 3, 17, 0, 6);
	}
}

void shortcuts_page_load_page(struct page *page)
{
	page->title = "Shortcut Configuration";
	page->draw_const = shortcuts_page_draw;
	page->set_page = NULL;
	page->pre_handle_key = shortcuts_page_handle_key;
	page->total_widgets = 5;
	page->selected_widget = 0;
	page->widgets = widgets_shortcuts;
	page->help_index = HELP_GLOBAL;
	{
		int count = shortcuts_count();
		int max_top = (count > SHORTCUTS_ROWS_VISIBLE) ? (count - SHORTCUTS_ROWS_VISIBLE) : 0;
		shortcuts_top = CLAMP(shortcuts_selected - 2, 0, max_top);
	}

	widget_create_other(widgets_shortcuts + 0, 0, shortcuts_page_handle_key, NULL, shortcuts_page_draw);
	widget_create_button(widgets_shortcuts + 1, 3, 16, 10, 0, 0, 4, 2, 2, shortcuts_action_rebind, "Rebind", 2);
	widget_create_button(widgets_shortcuts + 2, 15, 16, 12, 0, 0, 1, 3, 3, shortcuts_action_defaults, "Defaults", 2);
	widget_create_button(widgets_shortcuts + 3, 29, 16, 8, 0, 0, 2, 4, 4, shortcuts_action_save, "Save", 2);
	widget_create_button(widgets_shortcuts + 4, 39, 16, 8, 0, 0, 3, 1, 1, shortcuts_action_back, "Back", 2);
}
