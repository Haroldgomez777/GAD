#include "html_attr.h"
#include "gad_util.h"

#include <ctype.h>
#include <string.h>

static int attr_name_match_ci(const char *p, const char *aname)
{
	for (; *aname; p++, aname++) {
		if (!*p || gad_lower((unsigned char)*p) != gad_lower((unsigned char)*aname))
			return 0;
	}
	return *p == '=';
}

static const char *find_attr_eq(const char *tag, const char *aname)
{
	size_t alen = strlen(aname);
	const char *p;

	for (p = tag + 1; *p; p++) {
		if ((p == tag + 1 || isspace((unsigned char)p[-1]))
		    && attr_name_match_ci(p, aname))
			return gad_skip_space(p + alen + 1);
	}
	return NULL;
}

void html_extract_attr_ci(const char *tag, const char *aname, char *out, size_t outlen)
{
	const char *p = find_attr_eq(tag, aname);
	size_t i = 0;

	out[0] = '\0';
	if (!p)
		return;
	if (*p != '"' && *p != '\'')
		return;
	{
		char q = *p++;

		while (*p && *p != q && i + 1 < outlen)
			out[i++] = *p++;
		out[i] = '\0';
	}
}

void html_ascii_lower_str(char *dst, const char *src, size_t maxout)
{
	size_t i;

	for (i = 0; i + 1 < maxout && src[i]; i++)
		dst[i] = (char)gad_lower((unsigned char)src[i]);
	dst[i] = '\0';
}

int html_tag_has_boolean_attr(const char *tag, const char *aname)
{
	size_t alen = strlen(aname);
	const char *p;

	for (p = tag + 1; *p; p++) {
		if ((p == tag + 1 || isspace((unsigned char)p[-1]))
		    && attr_name_match_ci(p, aname)) {
			char c = p[alen];

			return c == '\0' || isspace((unsigned char)c) || c == '>'
			    || c == '/';
		}
	}
	return 0;
}
