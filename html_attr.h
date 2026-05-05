#ifndef HTML_ATTR_H
#define HTML_ATTR_H

#include "gad_config.h"

void html_extract_attr_ci(const char *tag, const char *aname, char *out, size_t outlen);
void html_ascii_lower_str(char *dst, const char *src, size_t maxout);
int html_tag_has_boolean_attr(const char *tag, const char *aname);

#endif
