#include "html_scan.h"
#include "gad_util.h"

#include <ctype.h>

static int starts_word_ci(const char *s, const char *word)
{
	for (; *word; s++, word++) {
		if (gad_lower((unsigned char)*s) != (unsigned char)*word)
			return 0;
	}
	return *s == '\0' || isspace((unsigned char)*s) || *s == '>' || *s == '/';
}

int html_is_open_script_or_style(const char *buf, int *is_style)
{
	const char *p = gad_skip_space(buf + 1);
	int closing = 0;

	if (*p == '/') {
		closing = 1;
		p = gad_skip_space(p + 1);
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

int html_is_close_script(const char *buf)
{
	const char *p = gad_skip_space(buf + 1);

	if (*p != '/')
		return 0;
	p = gad_skip_space(p + 1);
	return starts_word_ci(p, "script");
}

int html_is_close_style(const char *buf)
{
	const char *p = gad_skip_space(buf + 1);

	if (*p != '/')
		return 0;
	p = gad_skip_space(p + 1);
	return starts_word_ci(p, "style");
}

int html_is_open_head(const char *buf)
{
	const char *p = gad_skip_space(buf + 1);

	if (*p == '/')
		return 0;
	return starts_word_ci(p, "head");
}

int html_is_close_head(const char *buf)
{
	const char *p = gad_skip_space(buf + 1);

	if (*p != '/')
		return 0;
	p = gad_skip_space(p + 1);
	return starts_word_ci(p, "head");
}

int html_read_until_tag_end(FILE *fp, char *out, size_t outlen)
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

void html_skip_comment_after_open(FILE *fp)
{
	int c, prev = 0, prev2 = 0;

	while ((c = fgetc(fp)) != EOF) {
		if (prev2 == '-' && prev == '-' && c == '>')
			return;
		prev2 = prev;
		prev = c;
	}
}

int html_append_until_gt_quoted(FILE *fp, char *buf, size_t *pn, size_t maxlen)
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

int html_skip_embedded_until_close(FILE *fp, int mode)
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

			if (html_read_until_tag_end(fp, tag, sizeof(tag)) == EOF)
				return EOF;
			if (mode == 1) {
				if (html_is_close_style(tag))
					return 0;
			} else if (mode == 2) {
				if (html_is_close_head(tag))
					return 0;
			} else {
				if (html_is_close_script(tag))
					return 0;
			}
		}
	}
}
