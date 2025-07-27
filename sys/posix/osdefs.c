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

/* ugh */
#if defined(HAVE_POSIX_SPAWN) && defined(HAVE_WAITPID)
#include <spawn.h>
#include <sys/wait.h>

int posix_shell(const char *name, const char *arg)
{
	pid_t pid;
	char *local_name, *local_arg;
	char *argv[3];
	int r;

	local_name = strdup(name);
	local_arg = strdup(arg);

	argv[0] = local_name;
	argv[1] = local_arg;
	argv[2] = NULL;

	r = posix_spawn(&pid, name, 0, 0, argv, 0);

	waitpid(pid, &r, WUNTRACED);

	free(local_name);
	free(local_arg);

	return r;
}
#endif

#if defined(HAVE_EXECL) && defined(HAVE_FORK) && !defined(SCHISM_WIN32)
int posix_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	char *tmp;
	int st;
	pid_t fork_result_child_pid;

	fork_result_child_pid = fork();

	switch (fork_result_child_pid) {
	case -1:
		return 0;
	case 0:
		if (chdir(dir) == -1)
			_exit(255);
		if (asprintf(&tmp, "./%s", name) < 0)
			_exit(255);
		execl(tmp, tmp, maybe_arg, (char *)NULL);
		free(tmp);
		_exit(255);
	};

	while (waitpid(fork_result_child_pid, &st, 0) == -1);

	if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
		return 1;

	return 0;
}
#endif
