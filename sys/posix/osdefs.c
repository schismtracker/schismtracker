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

/* ugh */
extern char **environ;

int posix_exec(int *status, int *abnormal_exit, const char *dir, const char *name, ...)
{
	pid_t pid;
	char *argv[256]; /* more than enough */
	int r = 0;
	int i;

	{
		/* convert the variable args list */
		va_list ap;

		va_start(ap, name);

		argv[0] = str_dup(name);
		for (i = 1; i < 255; i++) {
			const char *arg = va_arg(ap, const char *);
			if (!arg)
				break;

			argv[i] = str_dup(arg);
		}
		argv[i] = NULL;

		va_end(ap);
	}

#if defined(HAVE_POSIX_SPAWN)
	{
		char *owd = dmoz_get_current_directory();

		if (dir && (chdir(dir) == -1))
			goto fail;

		if (posix_spawn(&pid, name, NULL, NULL, argv, environ) != 0)
			goto fail;

		/* hm */
		(void)chdir(owd);
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
		/* oops, execl wasn't supposed to return!
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
		if ((info.si_code == CLD_EXITED) || (info.si_code == CLD_KILLED) || (info.si_code == CLD_DUMPED)) {
			if (status)
				*status = info.si_status;
			if (abnormal_exit)
				*abnormal_exit = (info.si_code != CLD_EXITED);
			r = 1;
		}
	}
#elif defined(HAVE_WAITPID)
	{
		int st;

		/* older API; in virtually all POSIX versions */
		while (waitpid(pid, &st, 0) == -1);

		if (WIFEXITED(st)) {
			if (status)
				*status = WEXITSTATUS(st);
			r = 1;
		}
	}
#endif

fail: /* do NOT jump here in the child process in case of fork() */
	/* clean up the mess we've made */
	for (i = 0; argv[i]; i++)
		free(argv[i]);

	return r;
}

/* ------------------------------------------------------------------------------- */

int posix_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	int st;
	char *bat_name;

	if (asprintf(&bat_name, "./%s", name) < 0)
		return 0;

	if (!os_exec(&st, NULL, dir, bat_name, maybe_arg, (char *)NULL)) {
		free(bat_name);
		return 0; /* what? */
	}

	free(bat_name);

	return (st == 0);
}

#endif
