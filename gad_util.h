#ifndef GAD_UTIL_H
#define GAD_UTIL_H

int gad_lower(int c);
const char *gad_skip_space(const char *s);
int gad_str_eq_ci(const char *a, const char *b);
int gad_str_has_prefix_ci(const char *s, const char *pre);
int gad_ascii_ci_contains_substr(const char *s, const char *needle);

#endif
