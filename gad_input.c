#define _DEFAULT_SOURCE 1

#include "gad_input.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

int gad_is_http_url(const char *s)
{
	if (!s)
		return 0;
	return strncasecmp(s, "http://", 7) == 0 || strncasecmp(s, "https://", 8) == 0;
}

FILE *gad_open_input(const char *path_or_url, pid_t *out_fetch_child)
{
	int fd[2];
	pid_t pid;

	if (!out_fetch_child) {
		errno = EINVAL;
		return NULL;
	}
	*out_fetch_child = -1;
	if (!gad_is_http_url(path_or_url))
		return fopen(path_or_url, "rb");

	if (pipe(fd) < 0) {
		perror("gad: pipe");
		return NULL;
	}
	pid = fork();
	if (pid < 0) {
		perror("gad: fork");
		close(fd[0]);
		close(fd[1]);
		return NULL;
	}
	if (pid == 0) {
		close(fd[0]);
		if (dup2(fd[1], STDOUT_FILENO) < 0) {
			perror("gad: dup2");
			_exit(126);
		}
		close(fd[1]);
		execlp("curl", "curl", "-sSL", "-f", "--max-time", "60",
		    "--location", path_or_url, (char *)NULL);
		execlp("wget", "wget", "-q", "-O", "-", "-T", "60", "--max-redirect=5",
		    path_or_url, (char *)NULL);
		perror("gad: execlp (need curl or wget in PATH for URLs)");
		_exit(127);
	}
	close(fd[1]);
	*out_fetch_child = pid;
	return fdopen(fd[0], "rb");
}

int gad_close_input(FILE *fp, pid_t fetch_child)
{
	int st;

	if (fp && fclose(fp) != 0) {
		perror("gad: fclose");
		if (fetch_child >= 0)
			waitpid(fetch_child, NULL, 0);
		return -1;
	}
	if (fetch_child < 0)
		return 0;
	if (waitpid(fetch_child, &st, 0) < 0) {
		perror("gad: waitpid");
		return -1;
	}
	if (WIFEXITED(st) && WEXITSTATUS(st) == 0)
		return 0;
	if (WIFEXITED(st))
		fprintf(stderr, "gad: fetching URL failed (exit code %d)\n",
		    WEXITSTATUS(st));
	else
		fprintf(stderr, "gad: fetching URL failed\n");
	return -1;
}
