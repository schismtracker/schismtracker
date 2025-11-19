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
#include "osdefs.h"
#include "mem.h"
#include "dmoz.h"

/* WOW NICE */
#if ((defined(HAVE_EXECL) && defined(HAVE_FORK)) || defined(HAVE_POSIX_SPAWN)) && \
	(defined(HAVE_WAITID) || defined(HAVE_WAITPID)) && !defined(SCHISM_WIN32)
#include <sys/wait.h>
#if defined(HAVE_POSIX_SPAWN)
# include <spawn.h>
#endif

/* This is an older name for the same function. The "np" suffix
 * was removed after it was added to POSIX, but implementations
 * haven't caught up yet */
#if !defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR) && defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP)
# define HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR
# define posix_spawn_file_actions_addchdir posix_spawn_file_actions_addchdir_np
#endif

/* ugh */
extern char **environ;

int posix_exec(int *status, int *abnormal_exit, const char *dir, const char *name, ...)
{
	pid_t pid;
	char *argv[256]; /* more than enough */
	int r = 0;
	size_t i;

	/* Initialize this BEFORE anything else */
	if (abnormal_exit)
		*abnormal_exit = 0;

	{
		/* convert the variable args list */
		va_list ap;

		va_start(ap, name);

		argv[0] = name;
		for (i = 1; i < (ARRAY_SIZE(argv) - 1); i++) {
			char *arg = va_arg(ap, const char *);
			if (!arg)
				break;

			argv[i] = arg;
		}
		argv[i] = NULL;

		va_end(ap);
	}

#if defined(HAVE_POSIX_SPAWN)
	{
		if (dir) {
#if defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR)
			/* When this function is available we can use it
			 * to avoid an extra chdir() at the end */
			posix_spawn_file_actions_t actions;
			int r;

			if (posix_spawn_file_actions_init(&actions) != 0)
				goto fail;

			if (posix_spawn_file_actions_addchdir(&actions, dir) != 0)
				goto fail;

			r = posix_spawn(&pid, name, &actions, NULL, argv, environ);
			posix_spawn_file_actions_destroy(&actions);
			if (r != 0)
				goto fail;
#else
			char *owd = dmoz_get_current_directory();

			if (chdir(dir) == -1)
				goto fail;

			if (posix_spawn(&pid, name, NULL, NULL, argv, environ) != 0)
				goto fail;

			/* hm */
			(void)chdir(owd);
#endif
		} else {
			/* This is simple in comparison */
			if (posix_spawn(&pid, name, NULL, NULL, argv, environ) != 0)
				goto fail;
		}
	}
#elif defined(HAVE_FORK) && defined(HAVE_EXEC)
	pid = fork();
	switch (pid) {
	case -1:
		goto fail;
	case 0:
		/* running in the child process */
		if (dir && (chdir(dir) == -1))
			_exit(255);
		execv(name, argv);
		/* oops, execv wasn't supposed to return!
		 * couldn't exec the specified command name */
		_exit(255);
	};
#endif

	/* wait for the child process to finish */
#if defined(HAVE_WAITID)
	{
		siginfo_t info;

		/* newer API; POSIX.1-2001 */
		while (waitid(P_PID, pid, &info, WEXITED) == -1);

		/* if the child terminated abnormally, well, the exec call is still technically a success */
		switch (info.si_code) {
		case CLD_DUMPED:
		case CLD_KILLED:
			if (abnormal_exit)
				*abnormal_exit = 1;
			SCHISM_FALLTHROUGH;
		case CLD_EXITED:
			if (status)
				*status = info.si_status;
			r = 1;

			break;
		}
	}
#elif defined(HAVE_WAITPID)
	{
		int st;

		/* older API; in virtually all POSIX versions
		 *
		 * NOTE: old mac os x returns ECHILD if the process
		 * exits before we call waitpid(), hence we check
		 * explicitly for EINTR. */
		while (waitpid(pid, &st, 0) == -1 && errno == EINTR);

		if (WIFEXITED(st)) {
			if (status)
				*status = WEXITSTATUS(st);
		} else {
			if (abnormal_exit)
				*abnormal_exit = 1;

			/* if we're here, we probably received a signal that wasn't caught.
			 * this is MOST LIKELY going to be SIGBUS or SIGSEGV. However, we
			 * have no way to say which signal it was, so I'm ignoring it for now.
			 *
			 * OR... the process is stopped (i.e. Ctrl-Z), which shouldn't ever
			 * happen. */
		}

		r = 1;
	}
#endif

fail: /* do NOT jump here in the child process in case of fork() */
	return r;
}

/* ------------------------------------------------------------------------------- */

int posix_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	int st;
	int crash;
	char *bat_name;

	if (asprintf(&bat_name, "./%s", name) < 0)
		return 0;

	if (!os_exec(&st, &crash, dir, bat_name, maybe_arg, (char *)NULL)) {
		free(bat_name);
		return 0; /* what? */
	}

	free(bat_name);

	return (st == 0 && !crash);
}

#endif
