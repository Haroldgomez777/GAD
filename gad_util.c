#include "gad_util.h"

#include <ctype.h>

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
