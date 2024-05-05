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

/* This file is used for file format tables, load ordering, and declarations. It is intended to be included
after defining macros for handling the various file types listed here, and as such, it is not #ifdef guarded.

The type list should be arranged so that the types with the most specific checks are first, and the vaguest
ones are down at the bottom. This is to ensure that some lousy type doesn't "steal" files of a different type.
For example, if IT came before S3M, any S3M file starting with "IMPM" (which is unlikely, but possible, and
in fact quite easy to do) would be picked up by the IT check. In fact, Impulse Tracker itself has this problem.

Also, a format that might need to do a lot of work to tell if a file is of the right type (i.e. the MDL format
practically requires reading through the entire file to find the title block) should be down farther on the
list for performance purposes.

Don't rearrange the formats that are already here unless you have a VERY good reason to do so. I spent a good
3-4 hours reading all the format specifications, testing files, checking notes, and trying to break the
program by giving it weird files, and I'm pretty sure that this ordering won't fail unless you really try
doing weird stuff like hacking the files, but then you're just asking for trouble. ;) */


#ifndef READ_INFO
# define READ_INFO(x)
#endif
#ifndef LOAD_SONG
# define LOAD_SONG(x)
#endif
#ifndef SAVE_SONG
# define SAVE_SONG(x)
#endif
#ifndef LOAD_SAMPLE
# define LOAD_SAMPLE(x)
#endif
#ifndef SAVE_SAMPLE
# define SAVE_SAMPLE(x)
#endif
#ifndef LOAD_INSTRUMENT
# define LOAD_INSTRUMENT(x)
#endif
#ifndef SAVE_INSTRUMENT /* not actually used - instrument saving is currently hardcoded to write .iti files */
# define SAVE_INSTRUMENT(x)
#endif
#ifndef EXPORT
# define EXPORT(x)
#endif

/* --------------------------------------------------------------------------------------------------------- */

/* 669 has lots of checks to compensate for a really crappy 2-byte magic. (It's even a common English word
ffs... "if"?!) Still, it's better than STM. The only reason this is first is because the position of the
SCRM magic lies within the 669 message field, and the 669 check is much more complex (and thus more likely
to be right). */
READ_INFO(669) LOAD_SONG(669)

/* Since so many programs have added noncompatible extensions to the mod format, there are about 30 strings to
compare against for the magic. Also, there are special cases for WOW files, which even share the same magic
as plain ProTracker, but are quite different; there are some really nasty heuristics to detect these... ugh,
ugh, ugh. However, it has to be above the formats with the magic at the beginning...

This only handles 31-sample mods; 15-sample ones have no identifying
information and are therefore placed much lower in this list. */
READ_INFO(mod) LOAD_SONG(mod31) SAVE_SONG(mod)

/* S3M needs to be before a lot of stuff. */
READ_INFO(s3m) LOAD_SONG(s3m) SAVE_SONG(s3m)
/* FAR and S3M have different magic in the same place, so it doesn't really matter which one goes
where. I just have S3M first since it's a more common format. */
READ_INFO(far) LOAD_SONG(far)

/* These next formats have their magic at the beginning of the data, so none of them can possibly
conflict with other ones. I've organized them pretty much in order of popularity. */
READ_INFO(xm) LOAD_SONG(xm)
READ_INFO(it) LOAD_SONG(it) SAVE_SONG(it)
READ_INFO(mt2)
READ_INFO(mtm) LOAD_SONG(mtm)
READ_INFO(ntk)
READ_INFO(mdl) LOAD_SONG(mdl)
READ_INFO(med)
READ_INFO(okt) LOAD_SONG(okt)
READ_INFO(mid) LOAD_SONG(mid)
READ_INFO(mus) LOAD_SONG(mus)
READ_INFO(mf)

/* Sample formats with magic at start of file */
READ_INFO(its)  LOAD_SAMPLE(its)  SAVE_SAMPLE(its)
READ_INFO(au)   LOAD_SAMPLE(au)   SAVE_SAMPLE(au)
READ_INFO(aiff) LOAD_SAMPLE(aiff) SAVE_SAMPLE(aiff) EXPORT(aiff)
READ_INFO(wav)  LOAD_SAMPLE(wav)  SAVE_SAMPLE(wav)  EXPORT(wav)
#ifdef USE_FLAC
READ_INFO(flac) LOAD_SAMPLE(flac) SAVE_SAMPLE(flac) EXPORT(flac)
#endif
READ_INFO(iti)  LOAD_INSTRUMENT(iti)
READ_INFO(xi)   LOAD_INSTRUMENT(xi)
READ_INFO(pat)  LOAD_INSTRUMENT(pat)

READ_INFO(ult) LOAD_SONG(ult)
READ_INFO(liq)

READ_INFO(ams)
READ_INFO(f2r)

READ_INFO(s3i)  LOAD_SAMPLE(s3i)  SAVE_SAMPLE(s3i) /* FIXME should this be moved? S3I has magic at 0x4C... */

/* IMF and SFX (as well as STX) all have the magic values at 0x3C-0x3F, which is positioned in IT's
"reserved" field, Not sure about this positioning, but these are kind of rare formats anyway. */
READ_INFO(imf) LOAD_SONG(imf)
READ_INFO(sfx) LOAD_SONG(sfx)
READ_INFO(stx) LOAD_SONG(stx)

/* bleh */
#if defined(USE_NON_TRACKED_TYPES) && defined(HAVE_VORBIS)
READ_INFO(ogg)
#endif

/* STM seems to have a case insensitive magic string with several possible values, and only one byte
is guaranteed to be the same in the whole file... yeagh. */
READ_INFO(stm) LOAD_SONG(stm)

/* An ID3 tag could actually be anywhere in an MP3 file, and there's no guarantee that it even exists
at all. I might move this toward the top if I can figure out how to identify an MP3 more precisely. */
#ifdef USE_NON_TRACKED_TYPES
READ_INFO(mp3)
#endif

#if USE_MEDIAFOUNDATION
READ_INFO(win32mf)
#endif

/* 15-sample mods have literally no identifying information */
READ_INFO(mod) LOAD_SONG(mod15)

/* not really a type, so no info reader for these */
LOAD_SAMPLE(raw) SAVE_SAMPLE(raw)

/* --------------------------------------------------------------------------------------------------------- */

/* Clear these out so subsequent includes don't make an ugly mess */

#undef READ_INFO
#undef LOAD_SONG
#undef SAVE_SONG
#undef LOAD_SAMPLE
#undef SAVE_SAMPLE
#undef LOAD_INSTRUMENT
#undef SAVE_INSTRUMENT
#undef EXPORT

