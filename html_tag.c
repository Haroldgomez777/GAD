#include "html_tag.h"
#include "html_attr.h"
#include "gad_util.h"

#include <ctype.h>
#include <string.h>

static int role_value_is_chrome(const char *role)
{
	char r[ATTRBUF];

	html_ascii_lower_str(r, role, sizeof(r));
	return gad_str_eq_ci(r, "navigation") || gad_str_eq_ci(r, "complementary")
	    || gad_str_eq_ci(r, "banner") || gad_str_eq_ci(r, "contentinfo")
	    || gad_str_eq_ci(r, "search");
}

static int aria_hidden_is_true(const char *tag)
{
	char v[ATTRBUF];

	html_extract_attr_ci(tag, "aria-hidden", v, sizeof(v));
	html_ascii_lower_str(v, v, sizeof(v));
	return gad_str_eq_ci(v, "true") || gad_str_eq_ci(v, "1");
}

static int attr_contains_ci(const char *tag, const char *attr, const char *needle)
{
	char v[ATTRBUF];
	char vv[ATTRBUF];
	char nn[ATTRBUF];

	html_extract_attr_ci(tag, attr, v, sizeof(v));
	if (!v[0])
		return 0;
	html_ascii_lower_str(vv, v, sizeof(vv));
	html_ascii_lower_str(nn, needle, sizeof(nn));
	return strstr(vv, nn) != NULL;
}

static int class_or_id_contains_ui_token(const char *tag)
{
	static const char *tokens[] = {
		"ipshide",
		"ipspagination",
		"ipsshare",
		"elshare",
		"shareitem",
		"ipsfollow",
		"cguestteaser",
		"signin",
		"login",
		"ipscomment_tools",
		"ipscomment_controls",
		"ipsitemcontrols",
		NULL
	};
	int i;

	for (i = 0; tokens[i]; i++) {
		if (attr_contains_ci(tag, "class", tokens[i])
		    || attr_contains_ci(tag, "id", tokens[i]))
			return 1;
	}
	return 0;
}

int html_tag_is_closing(const char *tag)
{
	const char *p = gad_skip_space(tag + 1);

	return *p == '/';
}

void html_parse_tag_name(const char *tag, int is_close, char *out, size_t outlen)
{
	const char *p = tag + 1;

	p = gad_skip_space(p);
	if (is_close) {
		p = gad_skip_space(p + 1);
	} else if (*p == '/')
		return;
	for (; outlen > 1 && *p; p++) {
		if (!isalnum((unsigned char)*p) && *p != '-' && *p != ':')
			break;
		*out++ = (char)gad_lower((unsigned char)*p);
		outlen--;
	}
	*out = '\0';
}

int html_tag_void_or_selfclose(const char *tag, const char *name)
{
	static const char *voids[] = {
		"area", "base", "br", "col", "embed", "hr", "img", "input",
		"link", "meta", "param", "source", "track", "wbr", NULL
	};
	int i;
	const char *e;

	for (i = 0; voids[i]; i++) {
		if (gad_str_eq_ci(name, voids[i]))
			return 1;
	}
	e = tag + strlen(tag);
	while (e > tag && isspace((unsigned char)e[-1]))
		e--;
	if (e >= tag + 2 && e[-2] == '/' && e[-1] == '>')
		return 1;
	return 0;
}

int html_generic_skip_subtree(const char *tname, const char *tag)
{
	char role[ATTRBUF];

	if (gad_str_eq_ci(tname, "nav") || gad_str_eq_ci(tname, "aside")
	    || gad_str_eq_ci(tname, "footer") || gad_str_eq_ci(tname, "header")
	    || gad_str_eq_ci(tname, "figure") || gad_str_eq_ci(tname, "dialog")
	    || gad_str_eq_ci(tname, "template") || gad_str_eq_ci(tname, "math")
	    || gad_str_eq_ci(tname, "svg") || gad_str_eq_ci(tname, "button"))
		return 1;
	if (gad_str_eq_ci(tname, "link") || gad_str_eq_ci(tname, "meta")
	    || gad_str_eq_ci(tname, "img") || gad_str_eq_ci(tname, "input")
	    || gad_str_eq_ci(tname, "hr") || gad_str_eq_ci(tname, "br")
	    || gad_str_eq_ci(tname, "select") || gad_str_eq_ci(tname, "option")
	    || gad_str_eq_ci(tname, "textarea"))
		return 1;
	if (html_tag_has_boolean_attr(tag, "hidden"))
		return 1;
	if (aria_hidden_is_true(tag))
		return 1;
	if (attr_contains_ci(tag, "data-controller", "sharelink")
	    || attr_contains_ci(tag, "data-role", "pagejump")
	    || attr_contains_ci(tag, "data-role", "commentcontrols")
	    || attr_contains_ci(tag, "title", "sign in to follow this"))
		return 1;
	if (class_or_id_contains_ui_token(tag))
		return 1;
	html_extract_attr_ci(tag, "role", role, sizeof(role));
	if (role[0] && role_value_is_chrome(role))
		return 1;
	return 0;
}
