#ifndef GAD_INPUT_H
#define GAD_INPUT_H

#include <stdio.h>
#include <sys/types.h>

int gad_is_http_url(const char *s);
FILE *gad_open_input(const char *path_or_url, pid_t *out_fetch_child);
int gad_close_input(FILE *fp, pid_t fetch_child);

#endif
