#ifndef HTML_SCAN_H
#define HTML_SCAN_H

#include "gad_config.h"
#include <stdio.h>

int html_read_until_tag_end(FILE *fp, char *out, size_t outlen);
void html_skip_comment_after_open(FILE *fp);
int html_append_until_gt_quoted(FILE *fp, char *buf, size_t *pn, size_t maxlen);
int html_skip_embedded_until_close(FILE *fp, int mode);
int html_is_open_script_or_style(const char *buf, int *is_style);
int html_is_close_script(const char *buf);
int html_is_close_style(const char *buf);
int html_is_open_head(const char *buf);
int html_is_close_head(const char *buf);

#endif
