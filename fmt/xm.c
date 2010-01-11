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

#define NEED_BYTESWAP
#include "headers.h"
#include "slurp.h"
#include "fmt.h"

#include "it.h" // needed for get_effect_char (purely informational)
#include "log.h"
#include "sndfile.h"
#include "tables.h"

/* --------------------------------------------------------------------- */

int fmt_xm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 38 && memcmp(data, "Extended Module: ", 17) == 0))
                return false;

        file->description = "Fast Tracker 2 Module";
        file->type = TYPE_MODULE_XM;
        /*file->extension = str_dup("xm");*/
        file->title = calloc(21, sizeof(char));
        memcpy(file->title, data + 17, 20);
        file->title[20] = 0;
        return true;
}

/* --------------------------------------------------------------------------------------------------------- */

// gloriously stolen from xmp
struct xm_file_header {
        uint8_t id[17];         // ID text: "Extended module: "
        uint8_t name[20];       // Module name, padded with zeroes
        uint8_t doseof;         // 0x1a
        uint8_t tracker[20];    // Tracker name
        uint16_t version;       // Version number, minor-major
        uint32_t headersz;      // Header size
        uint16_t songlen;       // Song length (in patten order table)
        uint16_t restart;       // Restart position
        uint16_t channels;      // Number of channels (2,4,6,8,10,...,32)
        uint16_t patterns;      // Number of patterns (max 256)
        uint16_t instruments;   // Number of instruments (max 128)
        uint16_t flags;         // bit 0: 0=Amiga freq table, 1=Linear
        uint16_t tempo;         // Default tempo
        uint16_t bpm;           // Default BPM
};

static uint8_t autovib_import[8] = {
        VIB_SINE, VIB_SQUARE,
        VIB_RAMP_DOWN, // actually ramp up
        VIB_RAMP_DOWN, VIB_RANDOM,
        // default to sine
        VIB_SINE, VIB_SINE, VIB_SINE,
};


// note: this does NOT convert between volume and 'normal' effects, it only exchanges them
static void xm_swap_effects(MODCOMMAND *note)
{
        MODCOMMAND tmp = {
                .note = note->note,
                .instr = note->instr,
                .volcmd = note->command,
                .vol = note->param,
                .command = note->volcmd,
                .param = note->vol,
        };
        *note = tmp;
}

// convert volume column data from CMD_* to VOLCMD_*, if possible
// 1 = it was properly converted, 0 = couldn't do so without loss of information
static int xm_convert_voleffect(MODCOMMAND *note, int force)
{
        switch (note->volcmd) {
        case CMD_NONE:
                return 1;
        case CMD_VOLUME:
                note->volcmd = VOLCMD_VOLUME;
                note->vol = MIN(note->vol, 64);
                break;
        case CMD_PORTAMENTOUP:
                if (force)
                        note->vol = MIN(note->vol, 9);
                else if (note->vol > 9)
                        return 0;
                note->volcmd = VOLCMD_PORTAUP;
                break;
        case CMD_PORTAMENTODOWN:
                if (force)
                        note->vol = MIN(note->vol, 9);
                else if (note->vol > 9)
                        return 0;
                note->volcmd = VOLCMD_PORTADOWN;
                break;
        case CMD_TONEPORTAMENTO:
                if (note->vol >= 0xf0) {
                        // hack for people who can't type F twice :)
                        note->volcmd = VOLCMD_TONEPORTAMENTO;
                        note->vol = 0xff;
                        return 1;
                }
                for (int n = 0; n < 10; n++) {
                        if (force
                            ? (note->vol <= ImpulseTrackerPortaVolCmd[n])
                            : (note->vol == ImpulseTrackerPortaVolCmd[n])) {
                                note->volcmd = VOLCMD_TONEPORTAMENTO;
                                note->vol = n;
                                return 1;
                        }
                }
                return 0;
        case CMD_VIBRATO:
                if (force)
                        note->vol = MIN(note->vol, 9);
                else if (note->vol > 9)
                        return 0;
                note->volcmd = VOLCMD_VIBRATODEPTH;
                break;
        case CMD_FINEVIBRATO:
                if (force)
                        note->vol = 0;
                else if (note->vol)
                        return 0;
                note->volcmd = VOLCMD_VIBRATODEPTH;
                break;
        case CMD_PANNING:
                note->vol = MIN(64, note->vol * 64 / 255);
                note->volcmd = VOLCMD_PANNING;
                break;
        case CMD_VOLUMESLIDE:
                // ugh
                if (note->vol == 0)
                        return 0;
                if ((note->vol & 0xf) == 0) { // Dx0 / Cx
                        if (force)
                                note->vol = MIN(note->vol >> 4, 9);
                        else if ((note->vol >> 4) > 9)
                                return 0;
                        else
                                note->vol >>= 4;
                        note->volcmd = VOLCMD_VOLSLIDEUP;
                } else if ((note->vol & 0xf0) == 0) { // D0x / Dx
                        if (force)
                                note->vol = MIN(note->vol, 9);
                        else if (note->vol > 9)
                                return 0;
                        note->volcmd = VOLCMD_VOLSLIDEDOWN;
                } else if ((note->vol & 0xf) == 0xf) { // DxF / Ax
                        if (force)
                                note->vol = MIN(note->vol >> 4, 9);
                        else if ((note->vol >> 4) > 9)
                                return 0;
                        else
                                note->vol >>= 4;
                        note->volcmd = VOLCMD_FINEVOLUP;
                } else if ((note->vol & 0xf0) == 0xf0) { // DFx / Bx
                        if (force)
                                note->vol = MIN(note->vol, 9);
                        else if ((note->vol & 0xf) > 9)
                                return 0;
                        else
                                note->vol &= 0xf;
                        note->volcmd = VOLCMD_FINEVOLDOWN;
                } else { // ???
                        return 0;
                }
                break;
        case CMD_S3MCMDEX:
                switch (note->vol >> 4) {
                case 8:
                        /* Impulse Tracker imports XM volume-column panning very weirdly:
                                XM = P0 P1 P2 P3 P4 P5 P6 P7 P8 P9 PA PB PC PD PE PF
                                IT = 00 05 10 15 20 21 30 31 40 45 42 47 60 61 62 63
                        I'll be um, not duplicating that behavior. :) */
                        note->volcmd = VOLCMD_PANNING;
                        note->vol = SHORT_PANNING[note->vol & 0xf];
                        return 1;
                case 0: case 1: case 2: case 0xf:
                        if (force) {
                                note->volcmd = note->vol = 0;
                                return 1;
                        }
                        break;
                default:
                        break;
                }
                return 0;
        default:
                return 0;
        }
        return 1;
}


static void load_xm_patterns(CSoundFile *song, struct xm_file_header *hdr, slurp_t *fp)
{
        int pat, row, chan;
        uint32_t patlen;
        uint8_t b;
        uint16_t rows;
        uint16_t bytes;
        size_t end; // should be same data type as slurp_t's length
        MODCOMMAND *note;
        unsigned int lostpat = 0;
        unsigned int lostfx;

        for (pat = 0; pat < hdr->patterns; pat++) {
                lostfx = 0;

                slurp_read(fp, &patlen, 4); // = 8/9
                patlen = bswapLE32(patlen);
                b = slurp_getc(fp); // = 0
                if (hdr->version == 0x0102) {
                        rows = slurp_getc(fp) + 1;
                        patlen++; // fake it so that alignment works properly.
                } else {
                        slurp_read(fp, &rows, 2);
                        rows = bswapLE16(rows);
                }
                slurp_read(fp, &bytes, 2);
                bytes = bswapLE16(bytes); // if 0, pattern is empty

                slurp_seek(fp, patlen - 9, SEEK_CUR); // probably a no-op

                if (!rows)
                        continue;

                if (pat >= MAX_PATTERNS) {
                        if (bytes)
                                lostpat++;
                        slurp_seek(fp, bytes, SEEK_CUR);
                        continue;
                }

                note = song->Patterns[pat] = csf_allocate_pattern(rows, 64);
                song->PatternSize[pat] = song->PatternAllocSize[pat] = rows;

                if (!bytes)
                        continue;

                // hack to avoid having to count bytes when reading
                end = slurp_tell(fp) + bytes;
                end = MIN(end, fp->length);

                for (row = 0; row < rows; row++, note += MAX_CHANNELS - hdr->channels) {
                        for (chan = 0; fp->pos < end && chan < hdr->channels; chan++, note++) {
                                b = slurp_getc(fp);
                                if (b & 128) {
                                        if (b & 1) note->note = slurp_getc(fp);
                                        if (b & 2) note->instr = slurp_getc(fp);
                                        if (b & 4) note->vol = slurp_getc(fp);
                                        if (b & 8) note->command = slurp_getc(fp);
                                        if (b & 16) note->param = slurp_getc(fp);
                                } else {
                                        note->note = b;
                                        note->instr = slurp_getc(fp);
                                        note->vol = slurp_getc(fp);
                                        note->command = slurp_getc(fp);
                                        note->param = slurp_getc(fp);
                                }
                                // translate everything
                                if (note->note > 0 && note->note < 97)
                                        note->note += 12;
                                else if (note->note == 97)
                                        note->note = NOTE_OFF;
                                else
                                        note->note = NOTE_NONE;
                                if (note->command || note->param)
                                        csf_import_mod_effect(note, 1);
                                if (note->instr == 0xff)
                                        note->instr = 0;

                                // now that the mundane stuff is over with... NOW IT'S TIME TO HAVE SOME FUN!

                                // the volume column is initially imported as "normal" effects, juggled around
                                // in order to make it more IT-like, and then converted into volume-effects

                                /* IT puts all volume column effects into the effect column if there's not an
                                effect there already; in the case of two set-volume effects, the one in the
                                effect column takes precedence.
                                set volume with values > 64 are clipped to 64
                                pannings are imported as S8x, unless there's an effect in which case it's
                                translated to a volume-column panning value.
                                volume and panning slides with zero value (+0, -0, etc.) still translate to
                                an effect -- even though volslides don't have effect memory in FT2. */

                                switch (note->vol >> 4) {
                                case 5: // 0x50 = volume 64, 51-5F = nothing
                                        if (note->vol == 0x50) {
                                case 1 ... 4: // Set volume Value-$10
                                                note->volcmd = CMD_VOLUME;
                                                note->vol -= 0x10;
                                                break;
                                        } // NOTE: falls through from case 5 when vol != 0x50
                                case 0: // Do nothing
                                        note->volcmd = CMD_NONE;
                                        note->vol = 0;
                                        break;
                                case 6: // Volume slide down
                                        note->vol &= 0xf;
                                        if (note->vol)
                                                note->volcmd = CMD_VOLUMESLIDE;
                                        break;
                                case 7: // Volume slide up
                                        note->vol = (note->vol & 0xf) << 4;
                                        if (note->vol)
                                                note->volcmd = CMD_VOLUMESLIDE;
                                        break;
                                case 8: // Fine volume slide down
                                        note->vol &= 0xf;
                                        if (note->vol) {
                                                if (note->vol == 0xf)
                                                        note->vol = 0xe; // DFF is fine slide up...
                                                note->vol |= 0xf0;
                                                note->volcmd = CMD_VOLUMESLIDE;
                                        }
                                        break;
                                case 9: // Fine volume slide up
                                        note->vol = (note->vol & 0xf) << 4;
                                        if (note->vol) {
                                                note->vol |= 0xf;
                                                note->volcmd = CMD_VOLUMESLIDE;
                                        }
                                        break;
                                case 10: // Set vibrato speed
                                        /* ARGH. this doesn't actually CAUSE vibrato - it only sets the value!
                                        i don't think there's a way to handle this correctly and sanely, so
                                        i'll just do what impulse tracker and mpt do...
                                        (probably should write a warning saying the song might not be
                                        played correctly) */
                                        note->vol = (note->vol & 0xf) << 4;
                                        note->volcmd = CMD_VIBRATO;
                                        break;
                                case 11: // Vibrato
                                        note->vol &= 0xf;
                                        note->volcmd = CMD_VIBRATO;
                                        break;
                                case 12: // Set panning
                                        note->volcmd = CMD_S3MCMDEX;
                                        note->vol = 0x80 | (note->vol & 0xf);
                                        break;
                                case 13: // Panning slide left
                                        // in FT2, <0 sets the panning to far left on the SECOND tick
                                        // this is "close enough" (except at speed 1)
                                        note->vol &= 0xf;
                                        if (note->vol) {
                                                note->vol <<= 4;
                                                note->volcmd = CMD_PANNINGSLIDE;
                                        } else {
                                                note->vol = 0x80;
                                                note->volcmd = CMD_S3MCMDEX;
                                        }
                                        break;
                                case 14: // Panning slide right
                                        note->vol &= 0xf;
                                        if (note->vol)
                                                note->volcmd = CMD_PANNINGSLIDE;
                                        break;
                                case 15: // Tone porta
                                        note->vol = (note->vol & 0xf) << 4;
                                        note->volcmd = CMD_TONEPORTAMENTO;
                                        break;
                                }

                                if (note->command == CMD_KEYOFF && note->param == 0) {
                                        // FT2 ignores both K00 and its note entirely (but still plays
                                        // previous notes and processes the volume column!)
                                        note->note = NOTE_NONE;
                                        note->instr = 0;
                                        note->command = CMD_NONE;
                                } else if (note->note == NOTE_OFF && note->command == CMD_S3MCMDEX
                                           && (note->param >> 4) == 0xd) {
                                        // note off with a delay ignores the note off, and also
                                        // ignores set-panning (but not other effects!)
                                        // (actually the other vol. column effects happen on the
                                        // first tick with ft2, but this is "close enough" i think)
                                        note->note = NOTE_NONE;
                                        note->instr = 0;
                                        // note: haven't fixed up volumes yet
                                        if (note->volcmd == CMD_PANNING) {
                                                note->volcmd = CMD_NONE;
                                                note->vol = 0;
                                                note->command = CMD_NONE;
                                                note->param = 0;
                                        }
                                }

                                if (note->command == CMD_NONE && note->volcmd != CMD_NONE) {
                                        // put the lotion in the basket
                                        xm_swap_effects(note);
                                } else if (note->command == note->volcmd) {
                                        // two of the same kind of effect => ignore the volume column
                                        // (note that ft2 behaves VERY strangely with Mx + 3xx combined --
                                        // but i'll ignore that nonsense and just go by xm.txt here because
                                        // it's easier :)
                                        note->volcmd = note->vol = 0;
                                }
                                if (note->command == CMD_VOLUME) {
                                        // try to move set-volume into the volume column
                                        xm_swap_effects(note);
                                }
                                // now try to rewrite the volume column, if it's not possible then see if we
                                // can do so after swapping them.
                                // this is a terrible hack -- don't write code like this, kids :)
                                int n;
                                for (n = 0; n < 4; n++) {
                                        // (n >> 1) will be 0/1, indicating our desire to j... j... jam it in
                                        if (xm_convert_voleffect(note, n >> 1)) {
                                                n = 5; // it'd be nice if c had a for...else like python
                                                break;
                                        }
                                        // nope that didn't work, switch them around
                                        xm_swap_effects(note);
                                }
                                if (n < 5) {
                                        //log_appendf(4, "Warning: pat%u row%u chn%u: lost effect %c%02X",
                                        //      pat, row, chan + 1, get_effect_char(note->volcmd), note->vol);
                                        note->volcmd = note->vol = 0;
                                        lostfx++;
                                }

                                /* some XM effects that schism probably won't handle decently:
                                0xy / Jxy
                                  - this one is *totally* screwy, see milkytracker source for details :)
                                    (NOT documented -- in fact, all the documentation claims that it should
                                    simply play note -> note+x -> note+y -> note like any other tracker, but
                                    that sure isn't what FT2 does...)
                                Axy / Dxy
                                  - it's probably not such a good idea to move these between the volume and
                                    effect column, since there's a chance it might screw stuff up since the
                                    volslides don't share memory (in either .it or .xm) -- e.g.
                                        ... .. .. DF0
                                        ... .. .. D04
                                        ... .. .. D00
                                    is quite different from
                                        ... .. .. DF0
                                        ... .. D4 .00
                                        ... .. .. D00
                                    But oh well. Works "enough" for now.
                                E6x / SBx
                                  - ridiculously broken; it screws up the pattern break row if E60 isn't at
                                    the start of the pattern -- this is fairly well known by FT2 users, but
                                    curiously absent from its "known bugs" list
                                E9x / Q0x
                                  - actually E9x isn't like Q0x at all... it's really stupid, I give up.
                                    hope no one wants to listen to XM files with retrig.
                                ECx / SCx
                                  - doesn't actually CUT the note, it just sets volume to zero at tick x
                                    (this is documented) */
                        }
                }

                if (lostfx) {
                        log_appendf(2, "Pattern %d: %d effect%s skipped!",
                                pat, lostfx, lostfx == 1 ? "" : "s");
                }
        }

        if (lostpat)
                log_appendf(4, "Warning: too many patterns in song (%d skipped)", lostpat);
}

static void load_xm_samples(SONGSAMPLE *first, int total, slurp_t *fp)
{
        SONGSAMPLE *smp = first;
        size_t smpsize;
        int ns;

        // dontyou: 20 samples starting at 26122
        // trnsmix: 31 samples starting at 61946
        for (ns = 0; ns < total; ns++, smp++) {
                smpsize = smp->nLength;
                if (!smpsize)
                        continue;
                if (smp->uFlags & CHN_16BIT) {
                        smp->nLength >>= 1;
                        smp->nLoopStart >>= 1;
                        smp->nLoopEnd >>= 1;
                }
                // modplug's sample-reading function is complicated and retarded
                csf_read_sample(smp, SF_LE | SF_M | SF_PCMD | ((smp->uFlags & CHN_16BIT) ? SF_16 : SF_8),
                                fp->data + fp->pos, fp->length - fp->pos);
                slurp_seek(fp, smpsize, SEEK_CUR);
        }
}

enum {
        ID_CONFIRMED = 1, // confirmed with inst/sample header sizes
        ID_FT2GENERIC = 2, // "FastTracker v2.00", but fasttracker has NOT been ruled out
        ID_OLDMODPLUG = 4, // "FastTracker v 2.00"
        ID_OTHER = 8, // something we don't know, testing for digitrakker.
        ID_FT2CLONE = 16, // NOT FT2: itype changed between instruments, or \0 found in song title
        ID_MAYBEMODPLUG = 32, // some FT2-ish thing, possibly MPT.
        ID_DIGITRAK = 64, // probably digitrakker
        ID_UNKNOWN = 128 | ID_CONFIRMED, // ?????
};

// TODO: try to identify packers (boobiesqueezer?)

// this also does some tracker detection
// return value is the number of samples that need to be loaded later (for old xm files)
static int load_xm_instruments(CSoundFile *song, struct xm_file_header *hdr, slurp_t *fp)
{
        int n, ni, ns;
        int abssamp = 1; // "real" sample
        uint32_t ihdr, shdr; // instrument/sample header size
        uint8_t b;
        uint16_t w;
        uint32_t d;
        int detected;
        int itype = -1;
        uint8_t srsvd_or = 0; // bitwise-or of all sample reserved bytes

        if (strncmp(song->tracker_id, "FastTracker ", 12) == 0) {
                if (hdr->headersz == 276 && strncmp(song->tracker_id + 12, "v2.00   ", 8) == 0) {
                        // TODO: is it at all possible to tell the precise FT2 version? that'd be a neat trick.
                        // (Answer: unlikely. After some testing, I can't identify any differences between 2.04
                        // and 2.09. Doesn't mean for certain that they're identical, but I would be surprised
                        // if anything did change.)
                        detected = ID_FT2GENERIC | ID_MAYBEMODPLUG;
                        // replace the "v2.00" with just a 2, since it's probably not actually v2.00
                        strcpy(song->tracker_id + 12, "2");
                } else if (strncmp(song->tracker_id + 12, "v 2.00  ", 8) == 0) {
                        // Old MPT:
                        // - 1.00a5 (ihdr=245)
                        // - beta 3.3 (ihdr=263)
                        strcpy(song->tracker_id, "Modplug Tracker 1.0");
                        detected = ID_OLDMODPLUG;
                } else {
                        // definitely NOT FastTracker, so let's clear up that misconception
                        detected = ID_UNKNOWN;
                }
        } else if (strncmp(song->tracker_id, "*Converted ", 11) == 0 || strspn(song->tracker_id, " ") == 20) {
                // this doesn't catch any cases where someone typed something into the field :(
                detected = ID_OTHER | ID_DIGITRAK;
        } else {
                detected = ID_OTHER;
        }

        // FT2 pads the song title with spaces, some other trackers don't
        if (detected & ID_FT2GENERIC && memchr(song->song_title, '\0', 20) != NULL)
                detected = ID_FT2CLONE | ID_MAYBEMODPLUG;

        for (ni = 1; ni <= hdr->instruments; ni++) {
                int vtype, vsweep, vdepth, vrate;
                SONGINSTRUMENT *ins;
                uint16_t nsmp;

                slurp_read(fp, &ihdr, 4);
                ihdr = bswapLE32(ihdr);

                if (ni >= MAX_INSTRUMENTS) {
                        // TODO: try harder
                        log_appendf(4, "Warning: too many instruments in file");
                        break;
                }
                song->Instruments[ni] = ins = csf_allocate_instrument();

                slurp_read(fp, ins->name, 22);
                ins->name[22] = '\0';
                if ((detected & ID_DIGITRAK) && memchr(ins->name, '\0', 22) != NULL)
                        detected &= ~ID_DIGITRAK;

                b = slurp_getc(fp);
                if (itype == -1) {
                        itype = b;
                } else if (itype != b && (detected & ID_FT2GENERIC)) {
                        // FT2 writes some random junk for the instrument type field,
                        // but it's always the SAME junk for every instrument saved.
                        detected = (detected & ~ID_FT2GENERIC) | ID_FT2CLONE | ID_MAYBEMODPLUG;
                }
                slurp_read(fp, &nsmp, 2);
                nsmp = bswapLE16(nsmp);
                slurp_read(fp, &shdr, 4);
                shdr = bswapLE32(shdr);

                if (detected == ID_OLDMODPLUG) {
                        detected = ID_CONFIRMED;
                        if (ihdr == 245) {
                                strcat(song->tracker_id, " alpha");
                        } else if (ihdr == 263) {
                                strcat(song->tracker_id, " beta");
                        } else {
                                // WEIRD!!
                                detected = ID_UNKNOWN;
                        }
                }

                if (!nsmp) {
                        // lucky day! it's pretty easy to identify tracker if there's a blank instrument
                        if (!(detected & ID_CONFIRMED)) {
                                if ((detected & ID_MAYBEMODPLUG) && ihdr == 263 && shdr == 0) {
                                        detected = ID_CONFIRMED;
                                        strcpy(song->tracker_id, "Modplug Tracker");
                                } else if ((detected & ID_DIGITRAK) && ihdr != 29) {
                                        detected &= ~ID_DIGITRAK;
                                } else if ((detected & (ID_FT2CLONE | ID_FT2GENERIC)) && ihdr != 33) {
                                        // Sure isn't FT2.
                                        // note: FT2 NORMALLY writes shdr=40 for all samples, but sometimes it
                                        // just happens to write random garbage there instead. surprise!
                                        detected = ID_UNKNOWN;
                                }
                        }
                        // some adjustment hack from xmp.
                        slurp_seek(fp, ihdr - 33, SEEK_CUR);
                        continue;
                }

                for (n = 0; n < 12; n++)
                        ins->NoteMap[n] = n + 1;
                for (; n < 96 + 12; n++) {
                        ins->NoteMap[n] = n + 1;
                        ins->Keyboard[n] = slurp_getc(fp) + abssamp;
                }
                for (; n < 120; n++)
                        ins->NoteMap[n] = n + 1;

                // envelopes
                // (god, xm stores this in such a retarded format, why isn't all the volume stuff
                // together and THEN the panning, so that this could at least not be so redundant)

                int prevtick = -1;
                for (n = 0; n < 12; n++) {
                        slurp_read(fp, &w, 2); // tick
                        w = bswapLE16(w);
                        if (w < prevtick) {
                                // TODO: mikmod source indicates files exist with broken envelope values,
                                // and it does some complicated stuff to adjust them. investigate?
                                w = prevtick + 1;
                        }
                        ins->VolEnv.Ticks[n] = prevtick = w;
                        slurp_read(fp, &w, 2); // value
                        w = bswapLE16(w);
                        ins->VolEnv.Values[n] = MIN(w, 64);
                }
                // same thing again
                prevtick = -1;
                for (n = 0; n < 12; n++) {
                        slurp_read(fp, &w, 2); // tick
                        w = bswapLE16(w);
                        if (w < prevtick) {
                                w = prevtick + 1;
                        }
                        ins->PanEnv.Ticks[n] = prevtick = w;
                        slurp_read(fp, &w, 2); // value
                        w = bswapLE16(w);
                        ins->PanEnv.Values[n] = MIN(w, 64);
                }
                b = slurp_getc(fp);
                ins->VolEnv.nNodes = CLAMP(b, 2, 12);
                b = slurp_getc(fp);
                ins->PanEnv.nNodes = CLAMP(b, 2, 12);
                ins->VolEnv.nSustainStart = ins->VolEnv.nSustainEnd = slurp_getc(fp);
                ins->VolEnv.nLoopStart = slurp_getc(fp);
                ins->VolEnv.nLoopEnd = slurp_getc(fp);
                ins->PanEnv.nSustainStart = ins->PanEnv.nSustainEnd = slurp_getc(fp);
                ins->PanEnv.nLoopStart = slurp_getc(fp);
                ins->PanEnv.nLoopEnd = slurp_getc(fp);
                b = slurp_getc(fp);
                if (b & 1) ins->dwFlags |= ENV_VOLUME;
                if (b & 2) ins->dwFlags |= ENV_VOLSUSTAIN;
                if (b & 4) ins->dwFlags |= ENV_VOLLOOP;
                b = slurp_getc(fp);
                if (b & 1) ins->dwFlags |= ENV_PANNING;
                if (b & 2) ins->dwFlags |= ENV_PANSUSTAIN;
                if (b & 4) ins->dwFlags |= ENV_PANLOOP;

                vtype = autovib_import[slurp_getc(fp) & 0x7];
                vsweep = slurp_getc(fp);
                vdepth = slurp_getc(fp);
                vrate = slurp_getc(fp) / 4;

                slurp_read(fp, &w, 2);
                ins->nFadeOut = bswapLE16(w);

                // fix note-off
                if (!(ins->dwFlags & ENV_VOLUME) && ins->nFadeOut) {
                        ins->VolEnv.Ticks[0] = 0;
                        ins->VolEnv.Ticks[1] = 1;
                        ins->VolEnv.Values[0] = 64;
                        ins->VolEnv.Values[1] = 0;
                        ins->VolEnv.nNodes = 2;
                        ins->VolEnv.nSustainStart = ins->VolEnv.nSustainEnd = 0;
                        ins->dwFlags |= ENV_VOLUME | ENV_VOLSUSTAIN;
                }


                // some other things...
                ins->nPan = 128;
                ins->nGlobalVol = 128;
                ins->nPPC = 60; // C-5?

                /* here we're looking at what the ft2 spec SAYS are two reserved bytes.
                most programs blindly follow ft2's saving and add 22 zero bytes at the end (making
                the instrument header size 263 bytes), but ft2 is really writing the midi settings
                there, at least in the first 7 bytes. (as far as i can tell, the rest of the bytes
                are always zero) */
                int midi_enabled = slurp_getc(fp); // instrument midi enable = 0/1
                b = slurp_getc(fp); // midi transmit channel = 0-15
                ins->nMidiChannelMask = (midi_enabled == 1) ? 1 << MIN(b, 15) : 0;
                slurp_read(fp, &w, 2); // midi program = 0-127
                w = bswapLE16(w);
                ins->nMidiProgram = MIN(w, 127);
                slurp_read(fp, &w, 2); // bender range (halftones) = 0-36
                slurp_getc(fp); // mute computer = 0/1

                slurp_seek(fp, ihdr - 248, SEEK_CUR);

                for (ns = 0; ns < nsmp; ns++) {
                        int8_t relnote, finetune;
                        SONGSAMPLE *smp;

                        if (abssamp + ns >= MAX_SAMPLES) {
                                // TODO: try harder (fill unused sample slots)
                                log_appendf(4, "Warning: too many samples in file");
                                break;
                        }
                        smp = song->Samples + abssamp + ns;

                        slurp_read(fp, &d, 4);
                        smp->nLength = bswapLE32(d);
                        slurp_read(fp, &d, 4);
                        smp->nLoopStart = bswapLE32(d);
                        slurp_read(fp, &d, 4);
                        smp->nLoopEnd = bswapLE32(d) + smp->nLoopStart;
                        smp->nVolume = slurp_getc(fp);
                        smp->nVolume = MIN(64, smp->nVolume);
                        smp->nVolume *= 4; //mphack
                        smp->nGlobalVol = 64;
                        smp->uFlags = CHN_PANNING;
                        finetune = slurp_getc(fp);
                        b = slurp_getc(fp); // flags
                        if (smp->nLoopStart >= smp->nLoopEnd)
                                b &= ~3; // that loop sucks, turn it off
                        switch (b & 3) {
                                case 2: smp->uFlags |= CHN_PINGPONGLOOP;
                                case 1: smp->uFlags |= CHN_LOOP;
                        }
                        if (b & 0x10) {
                                smp->uFlags |= CHN_16BIT;
                                // NOTE length and loop start/end are adjusted later
                        }
                        smp->nPan = slurp_getc(fp); //mphack, should be adjusted to 0-64
                        relnote = slurp_getc(fp);
                        smp->nC5Speed = transpose_to_frequency(relnote, finetune);
                        srsvd_or |= slurp_getc(fp);
                        slurp_read(fp, smp->name, 22);
                        smp->name[22] = '\0';
                        if (detected & ID_DIGITRAK && memchr(smp->name, '\0', 22) != NULL)
                                detected &= ~ID_DIGITRAK;

                        smp->nVibType = vtype;
                        smp->nVibSweep = vsweep;
                        smp->nVibDepth = vdepth;
                        smp->nVibRate = vrate;
                }
                if (hdr->version == 0x0104)
                        load_xm_samples(song->Samples + abssamp, ns, fp);
                abssamp += ns;
                // if we ran out of samples, stop trying to load instruments
                // (note this will break things with xm format ver < 0x0104!)
                if (ns != nsmp)
                        break;
        }

        if (detected & ID_FT2CLONE) {
                if (srsvd_or == 0) {
                        strcpy(song->tracker_id, "Modplug Tracker");
                } else {
                        // PlayerPro: itype and smp rsvd are both always zero
                        // no idea how to identify it elsewise.
                        strcpy(song->tracker_id, "FastTracker clone");
                }
        } else if ((detected & ID_DIGITRAK) && srsvd_or == 0 && (itype ?: -1) == -1) {
                strcpy(song->tracker_id, "Digitrakker");
        } else if (detected == ID_UNKNOWN) {
                strcpy(song->tracker_id, "Unknown tracker");
        }

        return (hdr->version < 0x0104) ? abssamp : 0;
}

int fmt_xm_load_song(CSoundFile *song, slurp_t *fp, UNUSED unsigned int lflags)
{
        struct xm_file_header hdr;
        int n;
        uint8_t b;

        slurp_read(fp, &hdr, sizeof(hdr));
        hdr.version = bswapLE16(hdr.version);
        hdr.headersz = bswapLE32(hdr.headersz);
        hdr.songlen = bswapLE16(hdr.songlen);
        hdr.restart = bswapLE16(hdr.restart);
        hdr.channels = bswapLE16(hdr.channels);
        hdr.patterns = bswapLE16(hdr.patterns);
        hdr.instruments = bswapLE16(hdr.instruments);
        hdr.flags = bswapLE16(hdr.flags);
        hdr.tempo = bswapLE16(hdr.tempo);
        hdr.bpm = bswapLE16(hdr.bpm);

        if (memcmp(hdr.id, "Extended Module: ", 17) != 0 || hdr.doseof != 0x1a || hdr.channels > MAX_CHANNELS)
                return LOAD_UNSUPPORTED;

        memcpy(song->song_title, hdr.name, 20);
        song->song_title[20] = '\0';
        memcpy(song->tracker_id, hdr.tracker, 20);
        song->tracker_id[20] = '\0';

        if (hdr.flags & 1)
                song->m_dwSongFlags |= SONG_LINEARSLIDES;
        song->m_dwSongFlags |= SONG_ITOLDEFFECTS | SONG_COMPATGXX | SONG_INSTRUMENTMODE;
        song->m_nDefaultSpeed = MIN(hdr.tempo, 255) ?: 255;
        song->m_nDefaultTempo = CLAMP(hdr.bpm, 31, 255);
        song->m_nDefaultGlobalVolume = 128;
        song->m_nSongPreAmp = 48;

        for (n = 0; n < hdr.channels; n++)
                song->Channels[n].nPan = 32 * 4; //mphack
        for (; n < MAX_CHANNELS; n++)
                song->Channels[n].dwFlags |= CHN_MUTE;

        hdr.songlen = MIN(MAX_ORDERS, hdr.songlen);
        for (n = 0; n < hdr.songlen; n++) {
                b = slurp_getc(fp);
                song->Orderlist[n] = (b >= MAX_PATTERNS) ? ORDER_SKIP : b;
        }

        slurp_seek(fp, 60 + hdr.headersz, SEEK_SET);

        if (hdr.version == 0x0104) {
                load_xm_patterns(song, &hdr, fp);
                load_xm_instruments(song, &hdr, fp);
        } else {
                int nsamp = load_xm_instruments(song, &hdr, fp);
                load_xm_patterns(song, &hdr, fp);
                load_xm_samples(song->Samples + 1, nsamp, fp);
        }
        csf_insert_restart_pos(song, hdr.restart);

        return LOAD_SUCCESS;
}

