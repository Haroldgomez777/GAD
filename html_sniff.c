#include "html_sniff.h"
#include "html_attr.h"
#include "gad_util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
		out[o++] = (char)gad_lower(c);
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

void html_sniff_root_strategy(FILE *fp, enum html_root_strat *strat, char deep_tag[TNAMELEN])
{
	unsigned char *buf;
	long sz, rd;
	size_t i;
	size_t pos_main = (size_t)-1, pos_article = (size_t)-1, pos_body = (size_t)-1;
	size_t pos_role = (size_t)-1, pos_item = (size_t)-1;
	char tname[TNAMELEN], tmp_tag[TAGBUF];
	char deep_r[TNAMELEN] = "div", deep_i[TNAMELEN] = "div";

	deep_tag[0] = '\0';
	*strat = HTML_RS_FULLDOC;

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
		if (pos_main == (size_t)-1 && gad_str_eq_ci(tname, "main"))
			pos_main = i;
		if (pos_article == (size_t)-1 && gad_str_eq_ci(tname, "article"))
			pos_article = i;
		if (pos_body == (size_t)-1 && gad_str_eq_ci(tname, "body"))
			pos_body = i;
		sniff_buf_copy_tag((const char *)buf, i, (size_t)rd, tmp_tag, sizeof(tmp_tag));
		if (pos_role == (size_t)-1) {
			char role[ATTRBUF];

			html_extract_attr_ci(tmp_tag, "role", role, sizeof(role));
			html_ascii_lower_str(role, role, sizeof(role));
			if (gad_str_eq_ci(role, "main")) {
				pos_role = i;
				strncpy(deep_r, tname, TNAMELEN - 1);
				deep_r[TNAMELEN - 1] = '\0';
			}
		}
		if (pos_item == (size_t)-1) {
			char ip[ATTRBUF];

			html_extract_attr_ci(tmp_tag, "itemprop", ip, sizeof(ip));
			html_ascii_lower_str(ip, ip, sizeof(ip));
			if (gad_str_eq_ci(ip, "articlebody")) {
				pos_item = i;
				strncpy(deep_i, tname, TNAMELEN - 1);
				deep_i[TNAMELEN - 1] = '\0';
			}
		}
	}

	free(buf);

	if (pos_main != (size_t)-1) {
		*strat = HTML_RS_MAIN;
		return;
	}
	if (pos_article != (size_t)-1) {
		*strat = HTML_RS_ARTICLE;
		return;
	}
	if (pos_role != (size_t)-1) {
		*strat = HTML_RS_ROLEMAIN;
		strncpy(deep_tag, deep_r, TNAMELEN - 1);
		deep_tag[TNAMELEN - 1] = '\0';
		return;
	}
	if (pos_item != (size_t)-1) {
		*strat = HTML_RS_ITEMBODY;
		strncpy(deep_tag, deep_i, TNAMELEN - 1);
		deep_tag[TNAMELEN - 1] = '\0';
		return;
	}
	if (pos_body != (size_t)-1)
		*strat = HTML_RS_BODY;
}

int html_root_opener_matches_strat(enum html_root_strat strat, const char *tname,
    const char *tag, const char *deep_tag)
{
	char role[ATTRBUF], ip[ATTRBUF];

	switch (strat) {
	case HTML_RS_MAIN:
		return gad_str_eq_ci(tname, "main");
	case HTML_RS_ARTICLE:
		return gad_str_eq_ci(tname, "article");
	case HTML_RS_BODY:
		return gad_str_eq_ci(tname, "body");
	case HTML_RS_ROLEMAIN:
		html_extract_attr_ci(tag, "role", role, sizeof(role));
		html_ascii_lower_str(role, role, sizeof(role));
		return gad_str_eq_ci(role, "main") && gad_str_eq_ci(tname, deep_tag);
	case HTML_RS_ITEMBODY:
		html_extract_attr_ci(tag, "itemprop", ip, sizeof(ip));
		html_ascii_lower_str(ip, ip, sizeof(ip));
		return gad_str_eq_ci(ip, "articlebody") && gad_str_eq_ci(tname, deep_tag);
	default:
		return 0;
	}
}

int html_root_closer_matches_strat(enum html_root_strat strat, const char *tname,
    const char *deep_tag)
{
	switch (strat) {
	case HTML_RS_MAIN:
		return gad_str_eq_ci(tname, "main");
	case HTML_RS_ARTICLE:
		return gad_str_eq_ci(tname, "article");
	case HTML_RS_BODY:
		return gad_str_eq_ci(tname, "body");
	case HTML_RS_ROLEMAIN:
	case HTML_RS_ITEMBODY:
		return gad_str_eq_ci(tname, deep_tag);
	default:
		return 0;
	}
}
