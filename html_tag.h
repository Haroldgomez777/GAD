#ifndef HTML_TAG_H
#define HTML_TAG_H

#include "gad_config.h"

int html_tag_is_closing(const char *tag);
void html_parse_tag_name(const char *tag, int is_close, char *out, size_t outlen);
int html_tag_void_or_selfclose(const char *tag, const char *name);
int html_generic_skip_subtree(const char *tname, const char *tag);

#endif
