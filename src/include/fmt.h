/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

/* This should ONLY be included from title.c! */

#define FORMAT(t)\
        file_info *fmt_##t##_read_info\
                (const byte *data, size_t length, file_info *fi)

FORMAT(669);
FORMAT(ams);
FORMAT(dtm);
FORMAT(f2r);
FORMAT(far);
FORMAT(it);
FORMAT(liq);
FORMAT(mdl);
FORMAT(mod);
FORMAT(mt2);
FORMAT(mtm);
FORMAT(ntk);
FORMAT(rtm);
FORMAT(s3m);
FORMAT(stm);
FORMAT(ult);
FORMAT(xm);

#ifdef USE_NON_TRACKED_TYPES
FORMAT(sid);
FORMAT(mp3);
# ifdef HAVE_VORBIS
FORMAT(ogg);
# endif
#endif

#ifdef USE_SAMPLE_TYPES
FORMAT(its);
FORMAT(au);
#endif

#undef FORMAT

/* --------------------------------------------------------------------- */

typedef file_info *(*fmt_read_info_func) (const byte * data, size_t length, file_info * fi);

#define FILETYPE(t) fmt_##t##_read_info

/* The type list should be arranged so that the types with the most
 * specific checks are first, and the vaguest ones are down at the
 * bottom. This is to ensure that some lousy type doesn't "steal" files
 * of a different type. For example, if IT came before S3M, any S3M file
 * starting with "IMPM" (which is unlikely, but possible, and in fact
 * quite easy to do) would be picked up by the IT check. In fact,
 * Impulse Tracker itself has this problem.
 * 
 * Also, a format that might need to do a lot of work to tell if a file
 * is of the right type (i.e. the mdl format practically requires
 * reading through the entire file to find the title block) should be
 * down farther on the list for performance purposes.
 * 
 * Don't rearrange the formats that are already here unless you have a
 * VERY good reason to do so. I spent a good 3-4 hours reading all the
 * format specifications, testing files, checking notes, and trying to
 * break the program by giving it weird files, and I'm pretty sure that
 * this ordering won't fail unless you really try doing weird stuff like
 * hacking the files, but then you're just asking for trouble. ;) */

static const fmt_read_info_func types[] = {
        /* 669 has lots of checks to compensate for a really crappy
         * 2-byte magic. (it's even a common english word ffs... "if"?!)
         * still, it's better than stm. the only reason this is first is
         * because the position of the SCRM magic lies within the 669
         * message field, and the 669 check is much more complex and
         * thus more likely to be right ;) */
        FILETYPE(669),

        /* since so many programs have added noncompatible extensions to
         * the mod format, there are about 30 strings to compare against
         * for the magic. also, there are special cases for wow files,
         * which even share the same magic as plain protracker, but are
         * quite different; there are some really nasty heuristics to
         * detect these... ugh, ugh, ugh.
         * however, it has to be above the formats with the magic at the
         * beginning... */
        FILETYPE(mod),

        /* s3m needs to be before a lot of stuff. */
        FILETYPE(s3m),
        /* far and s3m have different magic in the same place, so it
         * doesn't really matter which one goes where. i just have s3m
         * first as it's a more common format. */
        FILETYPE(far),

        /* These next formats have their magic at the beginning of the
         * data, so none of them can possibly conflict with other ones.
         * I've organized them pretty much in order of popularity. */
        FILETYPE(xm),
        /* there's a bit of weirdness with some it files (including
         * "acid dreams" by legend, a demo song for some version)
         * requiring two different checks and three memcmp's. however,
         * since it's so widely used <opinion>'cuz impulse tracker
         * owns</opinion>, i'm putting it up here anyway. */
        FILETYPE(it),
        FILETYPE(mt2),
        FILETYPE(mtm),
        FILETYPE(ntk),
#ifdef USE_NON_TRACKED_TYPES
        FILETYPE(sid),  /* 6581 0wnz j00! */
#endif
        FILETYPE(mdl),

	/* dunno where to put these */
#ifdef USE_SAMPLE_TYPES
	FILETYPE(its),
	FILETYPE(au),
#endif

        FILETYPE(ult),
        FILETYPE(liq),
        /* i have NEVER seen any of these next four */
        FILETYPE(ams),
        FILETYPE(rtm),
        FILETYPE(f2r),
        FILETYPE(dtm),  /* not sure about the placement here */

        /* bleh */
#if defined(USE_NON_TRACKED_TYPES) && defined(HAVE_VORBIS)
        FILETYPE(ogg),
#endif

        /* stm seems to have a case insensitive magic string with
         * several possible values, and only one byte is guaranteed to
         * be the same in the whole file... yeagh */
        FILETYPE(stm),

        /* an id3 tag could actually be anywhere in an mp3 file, and
         * there's no guarantee that it even exists at all. i might move
         * this up toward the top if i can figure out how to identify an
         * mp3 more precisely. */
#ifdef USE_NON_TRACKED_TYPES
        FILETYPE(mp3),
#endif

        /* this needs to be at the bottom of the list! */
        NULL
};

#undef FILETYPE
