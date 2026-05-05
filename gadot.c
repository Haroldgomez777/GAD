/*
 * Convert HTML to plain text: visible body text only.
 * Strips tags; omits <head>...</head>; omits <script>/<style> bodies and
 * <!-- comments -->; declarations like <!DOCTYPE> are dropped.
 *
 * With -m (main content, works on any site):
 *   Chooses a content root by standard signals (first match by priority):
 *     <main>, <article>, role="main", itemprop="articleBody", then <body>.
 *   If none exist, uses the whole document (still drops nav/aside/footer
 *   regions). Skips common non-prose regions using element names and ARIA,
 *   not site-specific CSS classes.
 *
 *   -w is accepted as an alias for -m (backward compatible).
 *
 * Compile: gcc -o gad gadot.c -Wall -Wextra -std=c99
 * Usage:   gad [-m|-w] input.html [output.txt]
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAGBUF 2048
#define ATTRBUF 512
#define INNERMAX 512
#define TNAMELEN 16
#define SNIFF_MAX (16 * 1024 * 1024)

static int lower(int c)
{
	return (unsigned char)tolower((unsigned char)c);
}

static const char *skip_space(const char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

static int str_eq_ci(const char *a, const char *b)
{
	while (*a && *b) {
		if (lower((unsigned char)*a) != lower((unsigned char)*b))
			return 0;
		a++;
		b++;
	}
	return *a == *b;
}

static int starts_word_ci(const char *s, const char *word)
{
	for (; *word; s++, word++) {
		if (lower((unsigned char)*s) != (unsigned char)*word)
			return 0;
	}
	return *s == '\0' || isspace((unsigned char)*s) || *s == '>' || *s == '/';
}

static int is_open_script_or_style(const char *buf, int *is_style)
{
	const char *p = skip_space(buf + 1);
	int closing = 0;

	if (*p == '/') {
		closing = 1;
		p = skip_space(p + 1);
	}
	if (starts_word_ci(p, "script")) {
		if (!closing) {
			*is_style = 0;
			return 1;
		}
		return 0;
	}
	if (starts_word_ci(p, "style")) {
		if (!closing) {
			*is_style = 1;
			return 1;
		}
		return 0;
	}
	return 0;
}

static int is_close_script(const char *buf)
{
	const char *p = skip_space(buf + 1);

	if (*p != '/')
		return 0;
	p = skip_space(p + 1);
	return starts_word_ci(p, "script");
}

static int is_close_style(const char *buf)
{
	const char *p = skip_space(buf + 1);

	if (*p != '/')
		return 0;
	p = skip_space(p + 1);
	return starts_word_ci(p, "style");
}

static int is_open_head(const char *buf)
{
	const char *p = skip_space(buf + 1);

	if (*p == '/')
		return 0;
	return starts_word_ci(p, "head");
}

static int is_close_head(const char *buf)
{
	const char *p = skip_space(buf + 1);

	if (*p != '/')
		return 0;
	p = skip_space(p + 1);
	return starts_word_ci(p, "head");
}

static int read_until_tag_end(FILE *fp, char *out, size_t outlen)
{
	size_t n = 0;
	int quote = 0;
	int c;

	out[n++] = '<';
	while (n < outlen - 2) {
		c = fgetc(fp);
		if (c == EOF) {
			out[n] = '\0';
			return EOF;
		}
		out[n++] = (char)c;
		if (quote) {
			if (c == quote)
				quote = 0;
			continue;
		}
		if (c == '"' || c == '\'')
			quote = (int)(unsigned char)c;
		else if (c == '>')
			break;
	}
	out[n] = '\0';
	return 0;
}

static void skip_comment_after_open(FILE *fp)
{
	int c, prev = 0, prev2 = 0;

	while ((c = fgetc(fp)) != EOF) {
		if (prev2 == '-' && prev == '-' && c == '>')
			return;
		prev2 = prev;
		prev = c;
	}
}

static int append_until_gt_quoted(FILE *fp, char *buf, size_t *pn, size_t maxlen)
{
	size_t n = *pn;
	int quote = 0;
	int c;

	while (n < maxlen - 1) {
		c = fgetc(fp);
		if (c == EOF) {
			buf[n] = '\0';
			*pn = n;
			return EOF;
		}
		buf[n++] = (char)c;
		if (quote) {
			if (c == quote)
				quote = 0;
			*pn = n;
			continue;
		}
		if (c == '"' || c == '\'')
			quote = (int)(unsigned char)c;
		else if (c == '>') {
			buf[n] = '\0';
			*pn = n;
			return 0;
		}
		*pn = n;
	}
	buf[n] = '\0';
	return 0;
}

static int skip_embedded_until_close(FILE *fp, int mode)
{
	int c;

	for (;;) {
		c = fgetc(fp);
		if (c == EOF)
			return EOF;
		if (c != '<')
			continue;
		{
			char tag[TAGBUF];

			if (read_until_tag_end(fp, tag, sizeof(tag)) == EOF)
				return EOF;
			if (mode == 1) {
				if (is_close_style(tag))
					return 0;
			} else if (mode == 2) {
				if (is_close_head(tag))
					return 0;
			} else {
				if (is_close_script(tag))
					return 0;
			}
		}
	}
}

/* --- attribute helpers --- */

static int attr_name_match_ci(const char *p, const char *aname)
{
	for (; *aname; p++, aname++) {
		if (!*p || lower((unsigned char)*p) != lower((unsigned char)*aname))
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
			return skip_space(p + alen + 1);
	}
	return NULL;
}

static void extract_attr_ci(const char *tag, const char *aname, char *out, size_t outlen)
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

static void ascii_lower_str(char *dst, const char *src, size_t maxout)
{
	size_t i;

	for (i = 0; i + 1 < maxout && src[i]; i++)
		dst[i] = (char)lower((unsigned char)src[i]);
	dst[i] = '\0';
}

static int tag_is_closing(const char *tag)
{
	const char *p = skip_space(tag + 1);

	return *p == '/';
}

static void parse_tag_name(const char *tag, int is_close, char *out, size_t outlen)
{
	const char *p = tag + 1;

	p = skip_space(p);
	if (is_close) {
		p = skip_space(p + 1);
	} else if (*p == '/')
		return;
	for (; outlen > 1 && *p; p++) {
		if (!isalnum((unsigned char)*p) && *p != '-' && *p != ':')
			break;
		*out++ = (char)lower((unsigned char)*p);
		outlen--;
	}
	*out = '\0';
}

static int tag_void_or_selfclose(const char *tag, const char *name)
{
	static const char *voids[] = {
		"area", "base", "br", "col", "embed", "hr", "img", "input",
		"link", "meta", "param", "source", "track", "wbr", NULL
	};
	int i;
	const char *e;

	for (i = 0; voids[i]; i++) {
		if (str_eq_ci(name, voids[i]))
			return 1;
	}
	e = tag + strlen(tag);
	while (e > tag && isspace((unsigned char)e[-1]))
		e--;
	if (e >= tag + 2 && e[-2] == '/' && e[-1] == '>')
		return 1;
	return 0;
}

static int tag_has_boolean_attr(const char *tag, const char *aname)
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

static int role_value_is_chrome(const char *role)
{
	char r[ATTRBUF];

	ascii_lower_str(r, role, sizeof(r));
	return str_eq_ci(r, "navigation") || str_eq_ci(r, "complementary")
	    || str_eq_ci(r, "banner") || str_eq_ci(r, "contentinfo")
	    || str_eq_ci(r, "search");
}

static int aria_hidden_is_true(const char *tag)
{
	char v[ATTRBUF];

	extract_attr_ci(tag, "aria-hidden", v, sizeof(v));
	ascii_lower_str(v, v, sizeof(v));
	return str_eq_ci(v, "true") || str_eq_ci(v, "1");
}

/* Skip subtree: landmarks / chrome / non-text embeds (generic, no CSS classes). */
static int generic_skip_subtree(const char *tname, const char *tag)
{
	char role[ATTRBUF];

	if (str_eq_ci(tname, "nav") || str_eq_ci(tname, "aside")
	    || str_eq_ci(tname, "footer") || str_eq_ci(tname, "header")
	    || str_eq_ci(tname, "figure") || str_eq_ci(tname, "dialog")
	    || str_eq_ci(tname, "template") || str_eq_ci(tname, "math")
	    || str_eq_ci(tname, "svg"))
		return 1;
	if (str_eq_ci(tname, "link") || str_eq_ci(tname, "meta")
	    || str_eq_ci(tname, "img") || str_eq_ci(tname, "input")
	    || str_eq_ci(tname, "hr") || str_eq_ci(tname, "br"))
		return 1;
	if (tag_has_boolean_attr(tag, "hidden"))
		return 1;
	if (aria_hidden_is_true(tag))
		return 1;
	extract_attr_ci(tag, "role", role, sizeof(role));
	if (role[0] && role_value_is_chrome(role))
		return 1;
	return 0;
}

/* --- Sniff which root strategy to use (priority: main > article > role=main > itemprop > body) --- */

enum root_strat {
	RS_FULLDOC,
	RS_MAIN,
	RS_ARTICLE,
	RS_ROLEMAIN,
	RS_ITEMBODY,
	RS_BODY
};

static void buf_parse_opening_name(const char *buf, size_t i, size_t len, char *out,
    size_t outsz)
{
	size_t o = 0;

	if (i >= len || buf[i] != '<')
		goto end;
	i++;
	while (i < len && isspace((unsigned char)buf[i]))
		i++;
	if (i < len && buf[i] == '/')
		goto end;
	while (o + 1 < outsz && i < len) {
		unsigned char c = (unsigned char)buf[i];

		if (!isalnum(c) && c != '-' && c != ':')
			break;
		out[o++] = (char)lower(c);
		i++;
	}
end:
	out[o] = '\0';
}

static void sniff_buf_copy_tag(const char *buf, size_t from, size_t len, char *tag,
    size_t tagsz)
{
	size_t n = 0;
	int quote = 0;

	if (from >= len || tagsz < 4)
		return;
	tag[n++] = '<';
	if (buf[from] == '<')
		from++;
	for (; n + 1 < tagsz && from < len; from++) {
		int c = (unsigned char)buf[from];

		tag[n++] = (char)c;
		if (n == 2 && c == '!') {
			while (n + 1 < tagsz && from + 1 < len) {
				from++;
				c = (unsigned char)buf[from];
				tag[n++] = (char)c;
				if (c == '>')
					goto done;
			}
			goto done;
		}
		if (quote) {
			if (c == quote)
				quote = 0;
			continue;
		}
		if (c == '"' || c == '\'')
			quote = c;
		else if (c == '>')
			break;
	}
done:
	tag[n] = '\0';
}

static void sniff_root_strategy(FILE *fp, enum root_strat *strat, char deep_tag[TNAMELEN])
{
	unsigned char *buf;
	long sz, rd;
	size_t i;
	size_t pos_main = (size_t)-1, pos_article = (size_t)-1, pos_body = (size_t)-1;
	size_t pos_role = (size_t)-1, pos_item = (size_t)-1;
	char tname[TNAMELEN], tmp_tag[TAGBUF];
	char deep_r[TNAMELEN] = "div", deep_i[TNAMELEN] = "div";

	deep_tag[0] = '\0';
	*strat = RS_FULLDOC;

	if (fseek(fp, 0, SEEK_END) != 0)
		return;
	sz = ftell(fp);
	if (sz < 0 || fseek(fp, 0, SEEK_SET) != 0)
		return;
	if (sz > (long)SNIFF_MAX)
		sz = (long)SNIFF_MAX;
	buf = (unsigned char *)malloc((size_t)sz + 1);
	if (!buf)
		return;
	rd = fread(buf, 1, (size_t)sz, fp);
	buf[rd] = '\0';
	if (fseek(fp, 0, SEEK_SET) != 0) {
		free(buf);
		return;
	}

	for (i = 0; i + 1 < (size_t)rd; i++) {
		if (buf[i] != '<')
			continue;
		if (buf[i + 1] == '!' || buf[i + 1] == '?')
			continue;
		if (buf[i + 1] == '/') {
			i++;
			continue;
		}
		buf_parse_opening_name((const char *)buf, i, (size_t)rd, tname, sizeof(tname));
		if (!tname[0])
			continue;
		if (pos_main == (size_t)-1 && str_eq_ci(tname, "main"))
			pos_main = i;
		if (pos_article == (size_t)-1 && str_eq_ci(tname, "article"))
			pos_article = i;
		if (pos_body == (size_t)-1 && str_eq_ci(tname, "body"))
			pos_body = i;
		sniff_buf_copy_tag((const char *)buf, i, (size_t)rd, tmp_tag, sizeof(tmp_tag));
		if (pos_role == (size_t)-1) {
			char role[ATTRBUF];

			extract_attr_ci(tmp_tag, "role", role, sizeof(role));
			ascii_lower_str(role, role, sizeof(role));
			if (str_eq_ci(role, "main")) {
				pos_role = i;
				strncpy(deep_r, tname, TNAMELEN - 1);
				deep_r[TNAMELEN - 1] = '\0';
			}
		}
		if (pos_item == (size_t)-1) {
			char ip[ATTRBUF];

			extract_attr_ci(tmp_tag, "itemprop", ip, sizeof(ip));
			ascii_lower_str(ip, ip, sizeof(ip));
			if (str_eq_ci(ip, "articlebody")) {
				pos_item = i;
				strncpy(deep_i, tname, TNAMELEN - 1);
				deep_i[TNAMELEN - 1] = '\0';
			}
		}
	}

	free(buf);

	if (pos_main != (size_t)-1) {
		*strat = RS_MAIN;
		return;
	}
	if (pos_article != (size_t)-1) {
		*strat = RS_ARTICLE;
		return;
	}
	if (pos_role != (size_t)-1) {
		*strat = RS_ROLEMAIN;
		strncpy(deep_tag, deep_r, TNAMELEN - 1);
		deep_tag[TNAMELEN - 1] = '\0';
		return;
	}
	if (pos_item != (size_t)-1) {
		*strat = RS_ITEMBODY;
		strncpy(deep_tag, deep_i, TNAMELEN - 1);
		deep_tag[TNAMELEN - 1] = '\0';
		return;
	}
	if (pos_body != (size_t)-1)
		*strat = RS_BODY;
}

static int root_opener_matches_strat(enum root_strat strat, const char *tname,
    const char *tag, const char *deep_tag)
{
	char role[ATTRBUF], ip[ATTRBUF];

	switch (strat) {
	case RS_MAIN:
		return str_eq_ci(tname, "main");
	case RS_ARTICLE:
		return str_eq_ci(tname, "article");
	case RS_BODY:
		return str_eq_ci(tname, "body");
	case RS_ROLEMAIN:
		extract_attr_ci(tag, "role", role, sizeof(role));
		ascii_lower_str(role, role, sizeof(role));
		return str_eq_ci(role, "main") && str_eq_ci(tname, deep_tag);
	case RS_ITEMBODY:
		extract_attr_ci(tag, "itemprop", ip, sizeof(ip));
		ascii_lower_str(ip, ip, sizeof(ip));
		return str_eq_ci(ip, "articlebody") && str_eq_ci(tname, deep_tag);
	default:
		return 0;
	}
}

static int root_closer_matches_strat(enum root_strat strat, const char *tname,
    const char *deep_tag)
{
	switch (strat) {
	case RS_MAIN:
		return str_eq_ci(tname, "main");
	case RS_ARTICLE:
		return str_eq_ci(tname, "article");
	case RS_BODY:
		return str_eq_ci(tname, "body");
	case RS_ROLEMAIN:
	case RS_ITEMBODY:
		return str_eq_ci(tname, deep_tag);
	default:
		return 0;
	}
}

static void emit_visible_text(FILE *in, FILE *out, int main_mode)
{
	int c;
	char tag[TAGBUF];
	int is_style;
	size_t n;
	enum root_strat strat = RS_FULLDOC;
	char deep_tag[TNAMELEN] = "div";
	int in_content = !main_mode;
	int wait_root = 0;
	int root_depth = 0;
	int skip_region = 0;
	char inner_tags[INNERMAX][TNAMELEN];
	int inner_n = 0;

	if (main_mode) {
		sniff_root_strategy(in, &strat, deep_tag);
		if (strat == RS_FULLDOC)
			in_content = 1;
		else
			wait_root = 1;
	}

	while ((c = fgetc(in)) != EOF) {
		if (c != '<') {
			int emit = 1;

			if (main_mode && !in_content)
				emit = 0;
			if (main_mode && in_content && skip_region)
				emit = 0;
			if (emit)
				fputc(c, out);
			continue;
		}
		c = fgetc(in);
		if (c == EOF)
			break;
		if (c == '!') {
			int a = fgetc(in);
			int b = fgetc(in);

			if (a == '-' && b == '-') {
				skip_comment_after_open(in);
				continue;
			}
			n = 0;
			tag[n++] = '<';
			tag[n++] = '!';
			tag[n++] = (char)a;
			tag[n++] = (char)b;
			if (append_until_gt_quoted(in, tag, &n, sizeof(tag)) == EOF)
				break;
			continue;
		}
		if (c == '?') {
			n = 0;
			tag[n++] = '<';
			tag[n++] = '?';
			if (append_until_gt_quoted(in, tag, &n, sizeof(tag)) == EOF)
				break;
			continue;
		}
		if (ungetc(c, in) == EOF)
			break;
		if (read_until_tag_end(in, tag, sizeof(tag)) == EOF)
			break;
		is_style = 0;
		if (is_open_head(tag)) {
			if (skip_embedded_until_close(in, 2) == EOF)
				break;
			continue;
		}
		if (is_open_script_or_style(tag, &is_style)) {
			if (skip_embedded_until_close(in, is_style ? 1 : 0) == EOF)
				break;
			continue;
		}

		if (main_mode) {
			int is_close = tag_is_closing(tag);
			char tname[TNAMELEN];

			parse_tag_name(tag, is_close, tname, sizeof(tname));

			if (is_close) {
				if (skip_region) {
					if (inner_n > 0
					    && str_eq_ci(inner_tags[inner_n - 1], tname)) {
						inner_n--;
						if (inner_n == 0)
							skip_region = 0;
					}
					continue;
				}
				if (in_content && root_depth > 0
				    && root_closer_matches_strat(strat, tname, deep_tag)) {
					root_depth--;
					if (root_depth == 0)
						in_content = 0;
				}
				continue;
			}
			/* opening */
			if (wait_root) {
				if (root_opener_matches_strat(strat, tname, tag, deep_tag)) {
					wait_root = 0;
					in_content = 1;
					root_depth = 1;
				}
				continue;
			}
			if (!in_content)
				continue;
			if (tag_void_or_selfclose(tag, tname))
				continue;
			if (skip_region) {
				if (inner_n < INNERMAX) {
					size_t j;

					for (j = 0; j < TNAMELEN - 1 && tname[j]; j++)
						inner_tags[inner_n][j] = tname[j];
					inner_tags[inner_n][j] = '\0';
					inner_n++;
				}
				continue;
			}
			if (generic_skip_subtree(tname, tag)) {
				if (!tag_void_or_selfclose(tag, tname)) {
					skip_region = 1;
					if (inner_n < INNERMAX) {
						size_t j;

						for (j = 0; j < TNAMELEN - 1 && tname[j]; j++)
							inner_tags[inner_n][j] = tname[j];
						inner_tags[inner_n][j] = '\0';
						inner_n++;
					}
				}
				continue;
			}
			/* deepen root for nested same-type containers */
			if (root_depth > 0 && strat != RS_FULLDOC) {
				if (strat == RS_MAIN && str_eq_ci(tname, "main"))
					root_depth++;
				else if (strat == RS_ARTICLE && str_eq_ci(tname, "article"))
					root_depth++;
				else if (strat == RS_BODY && str_eq_ci(tname, "body"))
					root_depth++;
				else if ((strat == RS_ROLEMAIN || strat == RS_ITEMBODY)
				    && str_eq_ci(tname, deep_tag))
					root_depth++;
			}
			continue;
		}
	}
}

int main(int argc, char const *argv[])
{
	FILE *fp, *fp2;
	int main_mode = 0;
	int ai = 1;

	if (argc > 1 && (strcmp(argv[1], "-m") == 0 || strcmp(argv[1], "-w") == 0)) {
		main_mode = 1;
		ai = 2;
	}
	if (argc - ai < 1) {
		fprintf(stderr,
			"usage: %s [-m|-w] input.html [output.txt]\n"
			"  -m  Main content: HTML5 landmarks + ARIA (any site), not CSS-class rules\n"
			"  -w  Same as -m (alias)\n"
			"  If output is omitted, writes copy.txt\n",
			argv[0]);
		return 1;
	}

	fp = fopen(argv[ai], "rb");
	if (!fp) {
		perror(argv[ai]);
		return 1;
	}

	if (argc - ai < 2)
		fp2 = fopen("copy.txt", "wb");
	else
		fp2 = fopen(argv[ai + 1], "wb");

	if (!fp2) {
		perror(argc - ai < 2 ? "copy.txt" : argv[ai + 1]);
		fclose(fp);
		return 1;
	}

	emit_visible_text(fp, fp2, main_mode);

	fclose(fp2);
	fclose(fp);
	return 0;
}
