#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* just one global variable... */

int instrument_list_subpage = PAGE_INSTRUMENT_LIST_GENERAL;

/* --------------------------------------------------------------------- */
/* ... but tons o' ugly statics */

static struct item items_general[18];
static struct item items_volume[17];

static int subpage_switches_group[5] = { 1, 2, 3, 4, -1 };
static int nna_group[5] = { 6, 7, 8, 9, -1 };
static int dct_group[5] = { 10, 11, 12, 13, -1 };
static int dca_group[4] = { 14, 15, 16, -1 };

static int top_instrument = 1;
static int current_instrument = 1;
static int instrument_cursor_pos = 25;  /* "play" mode */

static int note_trans_top_line = 0;
static int note_trans_sel_line = 0;

static int note_trans_cursor_pos = 0;

/* shared by all the numentries on a page
 * (0 = volume, 1 = panning, 2 = pitch) */
static int numentry_cursor_pos[3] = { 0 };

/* --------------------------------------------------------------------- */

static void instrument_list_draw_list(void);

/* --------------------------------------------------------------------- */
/* the actual list */

static void instrument_list_reposition(void)
{
        if (current_instrument < top_instrument) {
                top_instrument = current_instrument;
                if (top_instrument < 1) {
                        top_instrument = 1;
                }
        } else if (current_instrument > top_instrument + 34) {
                top_instrument = current_instrument - 34;
        }
}

int instrument_get_current(void)
{
        return current_instrument;
}

void instrument_set(int n)
{
        int new_ins = n;

        if (page_is_instrument_list(status.current_page)) {
                new_ins = CLAMP(n, 1, 99);
        } else {
                new_ins = CLAMP(n, 0, 99);
        }

        if (current_instrument == new_ins)
                return;

        status.flags =
                (status.flags & ~SAMPLE_CHANGED) | INSTRUMENT_CHANGED;
        current_instrument = new_ins;
        instrument_list_reposition();

        status.flags |= NEED_UPDATE;
}

void instrument_synchronize_to_sample(void)
{
        song_instrument *ins;
        int sample = sample_get_current();
        int n, pos;

        /* 1. if the instrument with the same number as the current sample
         * has the sample in its sample_map, change to that instrument. */
        ins = song_get_instrument(sample, NULL);
        for (pos = 0; pos < 120; pos++) {
                if ((ins->sample_map[pos]) == sample) {
                        instrument_set(sample);
                        return;
                }
        }

        /* 2. look through the instrument list for the first instrument
         * that uses the selected sample. */
        for (n = 1; n < 100; n++) {
                if (n == sample)
                        continue;
                ins = song_get_instrument(n, NULL);
                for (pos = 0; pos < 120; pos++) {
                        if ((ins->sample_map[pos]) == sample) {
                                instrument_set(n);
                                return;
                        }
                }
        }

        /* 3. if no instruments are using sample, just change to the
         * same-numbered instrument. */
        instrument_set(sample);
}

/* --------------------------------------------------------------------- */

static int instrument_list_add_char(char c)
{
        char *name;

        if (c < 32)
                return 0;
        song_get_instrument(current_instrument, &name);
        text_add_char(name, c, &instrument_cursor_pos, 25);
        if (instrument_cursor_pos == 25)
                instrument_cursor_pos--;

        status.flags |= NEED_UPDATE;
        return 1;
}

static void instrument_list_delete_char(void)
{
        char *name;
        song_get_instrument(current_instrument, &name);
        text_delete_char(name, &instrument_cursor_pos, 25);

        status.flags |= NEED_UPDATE;
}

static void instrument_list_delete_next_char(void)
{
        char *name;
        song_get_instrument(current_instrument, &name);
        text_delete_next_char(name, &instrument_cursor_pos, 25);

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void instrument_list_draw_list(void)
{
        int pos, n;
        song_instrument *ins;
        int selected = (ACTIVE_PAGE.selected_item == 0);
        int is_current;
        byte buf[4];

        for (pos = 0, n = top_instrument; pos < 35; pos++, n++) {
                ins = song_get_instrument(n, NULL);
                is_current = (n == current_instrument);

                draw_text(numtostr_2(n, buf), 2, 13 + pos, 0, 2);
                if (instrument_cursor_pos < 25) {
                        /* it's in edit mode */
                        if (is_current) {
                                draw_text_len(ins->name, 25, 5, 13 + pos,
                                              6, 14);
                                if (selected) {
                                        draw_char(ins->
                                                  name
                                                  [instrument_cursor_pos],
                                                  5 +
                                                  instrument_cursor_pos,
                                                  13 + pos, 0, 3);
                                }
                        } else {
                                draw_text_len(ins->name, 25, 5, 13 + pos,
                                              6, 0);
                        }
                } else {
                        draw_text_len(ins->name, 25, 5, 13 + pos,
                                      ((is_current
                                        && selected) ? 0 : 6),
                                      (is_current ? (selected ? 3 : 14) :
                                       0));
                }
        }
}

static int instrument_list_handle_key_on_list(SDL_keysym * k)
{
        int new_ins = current_instrument;

        switch (k->sym) {
        case SDLK_UP:
                new_ins--;
                break;
        case SDLK_DOWN:
                new_ins++;
                break;
        case SDLK_PAGEUP:
                if (k->mod & KMOD_CTRL)
                        new_ins = 1;
                else
                        new_ins -= 16;
                break;
        case SDLK_PAGEDOWN:
                if (k->mod & KMOD_CTRL)
                        new_ins = 99;
                else
                        new_ins += 16;
                break;
        case SDLK_HOME:
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 0;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_END:
                if (instrument_cursor_pos < 24) {
                        instrument_cursor_pos = 24;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_LEFT:
                if (instrument_cursor_pos < 25
                    && instrument_cursor_pos > 0) {
                        instrument_cursor_pos--;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_RIGHT:
                if (instrument_cursor_pos == 25) {
                        change_focus_to(1);
                } else if (instrument_cursor_pos < 24) {
                        instrument_cursor_pos++;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 25;
                        status.flags |= NEED_UPDATE;
                } else {
                        printf("TODO: load-instrument page\n");
                }
                return 1;
        case SDLK_ESCAPE:
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 25;
                        status.flags |= NEED_UPDATE;
                        return 1;
                }
                return 0;
        case SDLK_BACKSPACE:
                if (instrument_cursor_pos == 25)
                        return 0;
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0)
                        instrument_list_delete_char();
                else if (k->mod & KMOD_CTRL)
                        instrument_list_add_char(127);
                return 1;
        case SDLK_DELETE:
                if (instrument_cursor_pos == 25)
                        return 0;
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0)
                        instrument_list_delete_next_char();
                return 1;
        default:
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0) {
                        char c = unicode_to_ascii(k->unicode);

                        if (c == 0)
                                return 0;

                        if (instrument_cursor_pos < 25) {
                                return instrument_list_add_char(c);
                        } else if (k->sym == SDLK_SPACE) {
                                instrument_cursor_pos = 0;
                                status.flags |= NEED_UPDATE;
                                return 1;
                        } else {
                                return kbd_play_note(c);
                        }
                }
                return 0;
        }

        new_ins = CLAMP(new_ins, 1, 99);
        if (new_ins != current_instrument) {
                instrument_set(new_ins);
                status.flags |= NEED_UPDATE;
        }
        return 1;
}

/* --------------------------------------------------------------------- */
/* note translation table */

static void note_trans_reposition(void)
{
        if (note_trans_sel_line < note_trans_top_line) {
                note_trans_top_line = note_trans_sel_line;
        } else if (note_trans_sel_line > note_trans_top_line + 31) {
                note_trans_top_line = note_trans_sel_line - 31;
        }
}

static void note_trans_draw(void)
{
        int pos, n;
        int is_selected = (ACTIVE_PAGE.selected_item == 5);
        int bg, sel_bg = (is_selected ? 14 : 0);
        song_instrument *ins =
                song_get_instrument(current_instrument, NULL);
        byte buf[4];

        SDL_LockSurface(screen);

        for (pos = 0, n = note_trans_top_line; pos < 32; pos++, n++) {
                bg = ((n == note_trans_sel_line) ? sel_bg : 0);

                /* invalid notes are translated to themselves
                 * (and yes, this edits the actual instrument) */
                if (ins->note_map[n] < 1 || ins->note_map[n] > 120)
                        ins->note_map[n] = n + 1;

                draw_text_unlocked(get_note_string(n + 1, buf), 32,
                                   16 + pos, 2, bg);
                draw_char_unlocked(168, 35, 16 + pos, 2, bg);
                draw_text_unlocked(get_note_string(ins->note_map[n], buf),
                                   36, 16 + pos, 2, bg);
                if (is_selected && n == note_trans_sel_line) {
                        if (note_trans_cursor_pos == 0)
                                draw_char_unlocked(buf[0], 36, 16 + pos, 0,
                                                   3);
                        else if (note_trans_cursor_pos == 1)
                                draw_char_unlocked(buf[2], 38, 16 + pos, 0,
                                                   3);
                }
                draw_char_unlocked(0, 39, 16 + pos, 2, bg);
                if (ins->sample_map[n]) {
                        numtostr_2(ins->sample_map[n], buf);
                } else {
                        buf[0] = buf[1] = 173;
                        buf[2] = 0;
                }
                draw_text_unlocked(buf, 40, 16 + pos, 2, bg);
                if (is_selected && n == note_trans_sel_line) {
                        if (note_trans_cursor_pos == 2)
                                draw_char_unlocked(buf[0], 40, 16 + pos, 0,
                                                   3);
                        else if (note_trans_cursor_pos == 3)
                                draw_char_unlocked(buf[1], 41, 16 + pos, 0,
                                                   3);
                }
        }

        SDL_UnlockSurface(screen);
}

static int note_trans_handle_key(SDL_keysym * k)
{
        int prev_line = note_trans_sel_line;
        int new_line = prev_line;
        int prev_pos = note_trans_cursor_pos;
        int new_pos = prev_pos;
        song_instrument *ins =
                song_get_instrument(current_instrument, NULL);
        char c = unicode_to_ascii(k->unicode);
        int n;

        switch (k->sym) {
        case SDLK_UP:
                if (--new_line < 0) {
                        change_focus_to(1);
                        return 1;
                }
                break;
        case SDLK_DOWN:
                new_line++;
                break;
        case SDLK_PAGEUP:
                if (k->mod & KMOD_CTRL) {
                        instrument_set(current_instrument - 1);
                        return 1;
                }
                new_line -= 16;
                break;
        case SDLK_PAGEDOWN:
                if (k->mod & KMOD_CTRL) {
                        instrument_set(current_instrument + 1);
                        return 1;
                }
                new_line += 16;
                break;
        case SDLK_HOME:
                new_line = 0;
                break;
        case SDLK_END:
                new_line = 119;
                break;
        case SDLK_LEFT:
                new_pos--;
                break;
        case SDLK_RIGHT:
                new_pos++;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                sample_set(ins->sample_map[note_trans_sel_line]);
                return 1;
        default:
                switch (note_trans_cursor_pos) {
                case 0:        /* note */
                        n = kbd_get_note(c);
                        if (n <= 0 || n > 120)
                                return 0;
                        ins->note_map[note_trans_sel_line] = n;
                        ins->sample_map[note_trans_sel_line] =
                                sample_get_current();
                        new_line++;
                        break;
                case 1:        /* octave */
                        if (c < '0' || c > '9')
                                return 0;
                        n = ins->note_map[note_trans_sel_line];
                        n = ((n - 1) % 12) + (12 * (c - '0')) + 1;
                        ins->note_map[note_trans_sel_line] = n;
                        new_line++;
                        break;
                case 2:        /* instrument, first digit */
                case 3:        /* instrument, second digit */
                        if (c == ' ') {
                                ins->sample_map[note_trans_sel_line] =
                                        sample_get_current();
                                new_line++;
                                break;
                        }
                        if (c == note_trans[NOTE_TRANS_CLEAR]) {
                                ins->sample_map[note_trans_sel_line] = 0;
                                sample_set(0);
                                new_line++;
                                break;
                        }
                        if (c < '0' || c > '9')
                                return 0;
                        c -= '0';
                        n = ins->sample_map[note_trans_sel_line];
                        if (note_trans_cursor_pos == 2) {
                                n = (c * 10) + (n % 10);
                                new_pos++;
                        } else {
                                n = ((n / 10) * 10) + c;
                                new_pos--;
                                new_line++;
                        }
                        ins->sample_map[note_trans_sel_line] = n;
                        sample_set(n);
                        break;
                }
                break;
        }

        new_line = CLAMP(new_line, 0, 119);
        note_trans_cursor_pos = CLAMP(new_pos, 0, 3);
        if (new_line != prev_line) {
                note_trans_sel_line = new_line;
                note_trans_reposition();
        }

        /* this causes unneeded redraws in some cases... oh well :P */
        status.flags |= NEED_UPDATE;
        return 1;
}

/* --------------------------------------------------------------------- */
/* volume envelope */

static void volume_envelope_draw(void)
{
        SDL_Rect envelope_rect = { 256, 144, 360, 64 };
        int is_selected = (ACTIVE_PAGE.selected_item == 5);
        int n;
        byte buf[16];

        draw_text("Volume Envelope", 33, 16, is_selected ? 3 : 0, 2);
        draw_fill_rect(&envelope_rect, 0);

        SDL_LockSurface(screen);

        for (n = 0; n < 64; n += 2)
                putpixel_screen(259, 144 + n, 12);
        for (n = 0; n < 256; n += 2)
                putpixel_screen(257 + n, 206, 12);
        // draw_line_screen(...);

        sprintf(buf, "Node %d/%d", 0, 2);
        draw_text_unlocked(buf, 66, 19, 2, 0);
        sprintf(buf, "Tick %d", 0);
        draw_text_unlocked(buf, 66, 21, 2, 0);
        sprintf(buf, "Value %d", 64);
        draw_text_unlocked(buf, 66, 23, 2, 0);

        SDL_UnlockSurface(screen);
}

static int volume_envelope_handle_key(SDL_keysym * k)
{
        switch (k->sym) {
        case SDLK_UP:
                change_focus_to(1);
                return 1;
        case SDLK_DOWN:
                change_focus_to(6);
                return 1;
        case SDLK_LEFT:
                return 1;
        case SDLK_RIGHT:
                return 1;
        default:
                return 0;
        }
}

/* --------------------------------------------------------------------- */
/* default key handler (for instrument changing on pgup/pgdn) */

static void instrument_list_handle_key(SDL_keysym * k)
{
        switch (k->sym) {
        case SDLK_PAGEUP:
                instrument_set(current_instrument - 1);
                break;
        case SDLK_PAGEDOWN:
                instrument_set(current_instrument + 1);
                break;
        default:
                return;
        }
}

/* --------------------------------------------------------------------- */

static void change_subpage(void)
{
        int item = ACTIVE_PAGE.selected_item;
        int page = status.current_page;

        switch (item) {
        case 1:
                page = PAGE_INSTRUMENT_LIST_GENERAL;
                break;
        case 2:
                page = PAGE_INSTRUMENT_LIST_VOLUME;
                break;
        case 3:
                //page = PAGE_INSTRUMENT_LIST_PANNING;
                break;
        case 4:
                //page = PAGE_INSTRUMENT_LIST_PITCH;
                break;
        default:
                return;
        }

        if (page != status.current_page) {
                pages[page].selected_item = item;
                pages[page].items[1].togglebutton.state =
                        pages[page].items[2].togglebutton.state =
                        pages[page].items[3].togglebutton.state =
                        pages[page].items[4].togglebutton.state = 0;
                pages[page].items[item].togglebutton.state = 1;
                set_page(page);
                instrument_list_subpage = page;
        }
}

/* --------------------------------------------------------------------- */
/* predraw hooks... */

static void instrument_list_general_predraw_hook(void)
{
        int n;
        song_instrument *ins =
                song_get_instrument(current_instrument, NULL);

        for (n = 6; n < 17; n++)
                items_general[n].togglebutton.state = 0;
        items_general[6 + ins->nna].togglebutton.state = 1;
        items_general[10 + ins->dct].togglebutton.state = 1;
        items_general[14 + ins->dca].togglebutton.state = 1;

        items_general[17].textentry.text = ins->filename;
}

static void instrument_list_volume_predraw_hook(void)
{
        song_instrument *ins =
                song_get_instrument(current_instrument, NULL);

        items_volume[6].toggle.state = !!(ins->flags & ENV_VOLUME);
        items_volume[7].toggle.state = !!(ins->flags & ENV_VOLCARRY);
        items_volume[8].toggle.state = !!(ins->flags & ENV_VOLLOOP);
        items_volume[11].toggle.state = !!(ins->flags & ENV_VOLSUSTAIN);

        items_volume[9].numentry.value = ins->vol_loop_start;
        items_volume[10].numentry.value = ins->vol_loop_end;
        items_volume[12].numentry.value = ins->vol_sustain_start;
        items_volume[13].numentry.value = ins->vol_sustain_end;

        /* mp hack: shifting values all over the place here, ugh */
        items_volume[14].thumbbar.value = ins->global_volume << 1;
        items_volume[15].thumbbar.value = ins->fadeout >> 5;
        items_volume[16].thumbbar.value = ins->volume_swing;
}

/* --------------------------------------------------------------------- */
/* update values in song */

static void instrument_list_general_update_values(void)
{
        song_instrument *ins =
                song_get_instrument(current_instrument, NULL);

        for (ins->nna = 0; ins->nna <= 3; ins->nna++)
                if (items_general[ins->nna + 6].togglebutton.state)
                        break;
        for (ins->dct = 0; ins->dct <= 3; ins->dct++)
                if (items_general[ins->dct + 10].togglebutton.state)
                        break;
        for (ins->dca = 0; ins->dca <= 2; ins->dca++)
                if (items_general[ins->dca + 14].togglebutton.state)
                        break;
}

static void instrument_list_volume_update_values(void)
{
        song_instrument *ins =
                song_get_instrument(current_instrument, NULL);

        /* $4 = {
         *     nFadeOut = 960, dwFlags = 0, nGlobalVol = 64, nPan = 128,
         *     VolPoints = {0, 100, 0 <repeats 30 times>},
         *     PanPoints = {0, 100, 0 <repeats 30 times>},
         *     PitchPoints = {0, 100, 0 <repeats 30 times>}, 
         *     VolEnv = "@@", '\000' <repeats 29 times>, 
         *     PanEnv = ' ' <repeats 25 times>, "\000\000\000\000\000\000",
         *     PitchEnv = ' ' <repeats 25 times>, "\000\000\000\000\000\000",
         *     sample_map = '\a' <repeats 120 times>,
         *         "\000\000\000\000\000\000\000",
         *     note_map = "\001\002\003\004\005\006\a\b\t\n\013\f\r\016\017"
         *         "\020\021\022\023\024\025\026\027\030\031\032\e\034\035"
         *         "\036\037 !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLM"
         *         "NOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwx\000\000"
         *         "\000\000\000\000\000",
         *     nVolEnv = 0 '\000', nPanEnv = 2 '\002', nPitchEnv = 2 '\002',
         *     nVolLoopStart = 0 '\000', nVolLoopEnd = 0 '\000',
         *     nVolSustainBegin = 0 '\000', nVolSustainEnd = 0 '\000', 
         *     nPanLoopStart = 0 '\000', nPanLoopEnd = 0 '\000', 
         *     nPanSustainBegin = 0 '\000', nPanSustainEnd = 0 '\000',
         *     nPitchLoopStart = 0 '\000', nPitchLoopEnd = 0 '\000', 
         *     nPitchSustainBegin = 0 '\000', nPitchSustainEnd = 0 '\000',
         *     nna = 3 '\003', dct = 0 '\000', dca = 0 '\000',
         *     nPanSwing = 0 '\000', nVolSwing = 0 '\000',
         *     nIFC = 0 '\000', nIFR = 0 '\000',
         *     wMidiBank = 0, nMidiProgram = 0 '\000',
         *     nMidiChannel = 0 '\000', nMidiDrumKey = 0 '\000',
         *     nPPS = 0 '\000', nPPC = 60 '<',
         *     name = "choir#", ' ' <repeats 19 times>,
         *         "\000\000\000\000\000\000",
         *     filename = '\000' <repeats 11 times>
         * } */

        ins->flags &=
                ~(ENV_VOLUME | ENV_VOLCARRY | ENV_VOLLOOP |
                  ENV_VOLSUSTAIN);
        if (items_volume[6].toggle.state)
                ins->flags |= ENV_VOLUME;
        if (items_volume[7].toggle.state)
                ins->flags |= ENV_VOLCARRY;
        if (items_volume[8].toggle.state)
                ins->flags |= ENV_VOLLOOP;
        if (items_volume[11].toggle.state)
                ins->flags |= ENV_VOLSUSTAIN;

        // vol_loop_start vol_loop_end vol_sustain_start vol_sustain_end
        ins->vol_loop_start = items_volume[9].numentry.value;
        ins->vol_loop_end = items_volume[10].numentry.value;
        ins->vol_sustain_start = items_volume[12].numentry.value;
        ins->vol_sustain_end = items_volume[13].numentry.value;

        /* more ugly shifts */
        ins->global_volume = items_volume[14].thumbbar.value >> 1;
        ins->fadeout = items_volume[15].thumbbar.value << 5;
        ins->volume_swing = items_volume[16].thumbbar.value;

        // 5 envelope
        // 9 loop begin
        // 10 loop end
        // 12 sus begin
        // 13 sus end
        // 14 global volume
        // 15 fade
        // 16 swing
}

/* --------------------------------------------------------------------- */
/* draw_const functions */

static void instrument_list_draw_const(void)
{
        draw_box(4, 12, 30, 48, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void instrument_list_general_draw_const(void)
{
        int n;

        instrument_list_draw_const();

        SDL_LockSurface(screen);

        draw_box_unlocked(31, 15, 42, 48,
                          BOX_THICK | BOX_INNER | BOX_INSET);

        /* Kind of a hack, and not really useful, but... :) */
        if (status.flags & CLASSIC_MODE) {
                draw_box_unlocked(55, 46, 73, 48,
                                  BOX_THICK | BOX_INNER | BOX_INSET);
                draw_text_unlocked("    ", 69, 47, 1, 0);
        } else {
                draw_box_unlocked(55, 46, 69, 48,
                                  BOX_THICK | BOX_INNER | BOX_INSET);
        }

        draw_text_unlocked("New Note Action", 54, 17, 0, 2);
        draw_text_unlocked("Duplicate Check Type & Action", 47, 32, 0, 2);
        draw_text_unlocked("Filename", 47, 47, 0, 2);

        for (n = 0; n < 35; n++) {
                draw_char_unlocked(134, 44 + n, 15, 0, 2);
                draw_char_unlocked(134, 44 + n, 30, 0, 2);
                draw_char_unlocked(154, 44 + n, 45, 0, 2);
        }

        SDL_UnlockSurface(screen);
}

static void instrument_list_volume_draw_const(void)
{
        instrument_list_draw_const();

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);

        SDL_LockSurface(screen);

        draw_box_unlocked(31, 17, 77, 26,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 27, 63, 30,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 31, 63, 35,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 36, 63, 40,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 41, 71, 44,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 45, 71, 47,
                          BOX_THICK | BOX_INNER | BOX_INSET);

        draw_text_unlocked("Volume Envelope", 38, 28, 0, 2);
        draw_text_unlocked("Carry", 48, 29, 0, 2);
        draw_text_unlocked("Envelope Loop", 40, 32, 0, 2);
        draw_text_unlocked("Loop Begin", 43, 33, 0, 2);
        draw_text_unlocked("Loop End", 45, 34, 0, 2);
        draw_text_unlocked("Sustain Loop", 41, 37, 0, 2);
        draw_text_unlocked("SusLoop Begin", 40, 38, 0, 2);
        draw_text_unlocked("SusLoop End", 42, 39, 0, 2);
        draw_text_unlocked("Global Volume", 40, 42, 0, 2);
        draw_text_unlocked("Fadeout", 46, 43, 0, 2);
        draw_text_unlocked("Volume Swing %", 39, 46, 0, 2);

        SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */
/* load_page functions */

void instrument_list_general_load_page(struct page *page)
{
        page->title = "Instrument List (F4)";
        page->draw_const = instrument_list_general_draw_const;
        page->predraw_hook = instrument_list_general_predraw_hook;
        page->handle_key = instrument_list_handle_key;
        page->total_items = 18;
        page->items = items_general;
        page->help_index = HELP_INSTRUMENT_LIST;

        /* the first five items are the same for all four pages. */

        /* 0 = instrument list */
        items_general[0].type = ITEM_OTHER;
        items_general[0].next.tab = 1;
        items_general[0].other.handle_key =
                instrument_list_handle_key_on_list;
        items_general[0].other.redraw = instrument_list_draw_list;
        /* 1-4 = subpage switches */
        create_togglebutton(items_general + 1, 32, 13, 7, 1, 5, 0, 2, 2,
                            change_subpage, "General", 1,
                            subpage_switches_group);
        create_togglebutton(items_general + 2, 44, 13, 7, 2, 6, 1, 3, 3,
                            change_subpage, "Volume", 1,
                            subpage_switches_group);
        create_togglebutton(items_general + 3, 56, 13, 7, 3, 6, 2, 4, 4,
                            change_subpage, "Panning", 1,
                            subpage_switches_group);
        create_togglebutton(items_general + 4, 68, 13, 7, 4, 6, 3, 0, 0,
                            change_subpage, "Pitch", 2,
                            subpage_switches_group);
        items_general[1].togglebutton.state = 1;

        /* 5 = note trans table */
        items_general[5].type = ITEM_OTHER;
        items_general[5].next.tab = 6;
        items_general[5].other.handle_key = note_trans_handle_key;
        items_general[5].other.redraw = note_trans_draw;
        /* 6-9 = nna toggles */
        create_togglebutton(items_general + 6, 46, 19, 29, 2, 7, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Cut", 2, nna_group);
        create_togglebutton(items_general + 7, 46, 22, 29, 6, 8, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Continue", 2, nna_group);
        create_togglebutton(items_general + 8, 46, 25, 29, 7, 9, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Off", 2, nna_group);
        create_togglebutton(items_general + 9, 46, 28, 29, 8, 10, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Fade", 2, nna_group);
        /* 10-13 = dct toggles */
        create_togglebutton(items_general + 10, 46, 34, 12, 9, 11, 5, 14,
                            14, instrument_list_general_update_values,
                            "Disabled", 2, dct_group);
        create_togglebutton(items_general + 11, 46, 37, 12, 10, 12, 5, 15,
                            15, instrument_list_general_update_values,
                            "Note", 2, dct_group);
        create_togglebutton(items_general + 12, 46, 40, 12, 11, 13, 5, 16,
                            16, instrument_list_general_update_values,
                            "Sample", 2, dct_group);
        create_togglebutton(items_general + 13, 46, 43, 12, 12, 17, 5, 13,
                            13, instrument_list_general_update_values,
                            "Instrument", 2, dct_group);
        /* 14-16 = dca toggles */
        create_togglebutton(items_general + 14, 62, 34, 13, 9, 15, 10, 0,
                            0, instrument_list_general_update_values,
                            "Note Cut", 2, dca_group);
        create_togglebutton(items_general + 15, 62, 37, 13, 14, 16, 11, 0,
                            0, instrument_list_general_update_values,
                            "Note Off", 2, dca_group);
        create_togglebutton(items_general + 16, 62, 40, 13, 15, 17, 12, 0,
                            0, instrument_list_general_update_values,
                            "Note Fade", 2, dca_group);
        /* 17 = filename */
        /* impulse tracker has a 17-char-wide box for the filename for
         * some reason, though it still limits the actual text to 12
         * characters. go figure... */
        create_textentry(items_general + 17, 56, 47, 13, 13, 17, 0, NULL,
                         NULL, NULL, 12);
}

void instrument_list_volume_load_page(struct page *page)
{
        page->title = "Instrument List (F4)";
        page->draw_const = instrument_list_volume_draw_const;
        page->predraw_hook = instrument_list_volume_predraw_hook;
        page->handle_key = instrument_list_handle_key;
        page->total_items = 17;
        page->items = items_volume;
        page->help_index = HELP_INSTRUMENT_LIST;

        /* the first five items are the same for all four pages. */

        /* 0 = instrument list */
        items_volume[0].type = ITEM_OTHER;
        items_volume[0].next.tab = 1;
        items_volume[0].other.handle_key =
                instrument_list_handle_key_on_list;
        items_volume[0].other.redraw = instrument_list_draw_list;
        /* 1-4 = subpage switches */
        create_togglebutton(items_volume + 1, 32, 13, 7, 1, 5, 0, 2, 2,
                            change_subpage, "General", 1,
                            subpage_switches_group);
        create_togglebutton(items_volume + 2, 44, 13, 7, 2, 5, 1, 3, 3,
                            change_subpage, "Volume", 1,
                            subpage_switches_group);
        create_togglebutton(items_volume + 3, 56, 13, 7, 3, 5, 2, 4, 4,
                            change_subpage, "Panning", 1,
                            subpage_switches_group);
        create_togglebutton(items_volume + 4, 68, 13, 7, 4, 5, 3, 0, 0,
                            change_subpage, "Pitch", 2,
                            subpage_switches_group);

        /* 5 = volume envelope */
        items_volume[5].type = ITEM_OTHER;
        items_volume[5].next.tab = 0;
        items_volume[5].other.handle_key = volume_envelope_handle_key;
        items_volume[5].other.redraw = volume_envelope_draw;

        /* 6-7 = envelope switches */
        create_toggle(items_volume + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_toggle(items_volume + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_volume_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(items_volume + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_numentry(items_volume + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);
        create_numentry(items_volume + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);

        /* 11-13 = susloop settings */
        create_toggle(items_volume + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_numentry(items_volume + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);
        create_numentry(items_volume + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);

        /* 14-16 = volume thumbbars */
        create_thumbbar(items_volume + 14, 54, 42, 17, 13, 15, 0,
                        instrument_list_volume_update_values, 0, 128);
        create_thumbbar(items_volume + 15, 54, 43, 17, 14, 16, 0,
                        instrument_list_volume_update_values, 0, 128);
        create_thumbbar(items_volume + 16, 54, 46, 17, 15, 16, 0,
                        instrument_list_volume_update_values, 0, 100);
}
