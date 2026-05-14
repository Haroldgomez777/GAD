#include "gad_util.h"

#include <ctype.h>
#include <stddef.h>

int gad_lower(int c)
{
	return (unsigned char)tolower((unsigned char)c);
}

const char *gad_skip_space(const char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

int gad_str_eq_ci(const char *a, const char *b)
{
	while (*a && *b) {
		if (gad_lower((unsigned char)*a) != gad_lower((unsigned char)*b))
			return 0;
		a++;
		b++;
	}
	return *a == *b;
}

int gad_str_has_prefix_ci(const char *s, const char *pre)
{
	for (; *pre; s++, pre++) {
		if (!*s)
			return 0;
		if (gad_lower((unsigned char)*s) != gad_lower((unsigned char)*pre))
			return 0;
	}
	return 1;
}

int gad_ascii_ci_contains_substr(const char *s, const char *needle)
{
	size_t i, j;

	if (!needle[0])
		return 1;
	for (i = 0; s[i]; i++) {
		for (j = 0; needle[j]; j++) {
			unsigned char sc = (unsigned char)s[i + j];
			unsigned char nc = (unsigned char)needle[j];

			if (!sc)
				return 0;
			if (gad_lower((int)sc) != gad_lower((int)nc))
				break;
		}
		if (!needle[j])
			return 1;
	}
	return 0;
}
