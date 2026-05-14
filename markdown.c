#include "markdown.h"
#include "html_attr.h"
#include "gad_util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MD_STAMP_TRIM_MAX MD_LINE_DEDUP_MAX

void md_init(MdCtx *m, FILE *out, int enabled)
{
	memset(m, 0, sizeof(*m));
	m->out = out;
	m->enabled = enabled;
}

int md_active_for_tag(int main_mode, int in_content, int skip_region, int wait_root)
{
	if (!main_mode)
		return 1;
	if (wait_root || !in_content || skip_region)
		return 0;
	return 1;
}

static int md_line_is_post_stamp(const char *trimmed, size_t tl)
{
	if (tl == 0)
		return 0;
	if (tl > 96)
		return 0;
	if (tl < 12)
		return 0;
	if (!gad_str_has_prefix_ci(trimmed, "Posted "))
		return 0;
	if (gad_ascii_ci_contains_substr(trimmed, "http")
	    || gad_ascii_ci_contains_substr(trimmed, "www."))
		return 0;
	return 1;
}

static int md_line_trim_copy(const char *in, size_t inlen, char *outbuf, size_t outsz,
    size_t *trimlen_io)
{
	size_t lo = 0, hi = inlen;

	while (lo < hi && isspace((unsigned char)in[lo]))
		lo++;
	while (hi > lo && isspace((unsigned char)in[hi - 1]))
		hi--;

	if (hi - lo >= outsz)
		return -1;
	memcpy(outbuf, in + lo, hi - lo);
	outbuf[hi - lo] = '\0';
	*trimlen_io = hi - lo;
	return 0;
}

static void md_lnbuf_partial_flush(MdCtx *m)
{
	if (m->lnlen == 0)
		return;
	fwrite(m->lnbuf, 1, m->lnlen, m->out);
	m->lnlen = 0;
	m->stamp_prev_valid = 0;
}

static void md_lnbuf_emit_line_finish(MdCtx *m);

void md_emit_char(MdCtx *m, int c)
{
	unsigned char uc = (unsigned char)c;

	if (!m->enabled)
		return;
	if (m->pre) {
		fputc((int)uc, m->out);
		return;
	}
	if (uc == '\n') {
		md_lnbuf_emit_line_finish(m);
		return;
	}
	if (m->lnlen >= sizeof(m->lnbuf) - (size_t)1) {
		md_lnbuf_partial_flush(m);
		fputc((int)uc, m->out);
		return;
	}
	m->lnbuf[m->lnlen++] = (char)uc;
}

static void md_emit_str_impl(MdCtx *m, const char *s)
{
	while (*s)
		md_emit_char(m, (unsigned char)*s++);
}

static void md_puts(MdCtx *m, const char *s)
{
	md_emit_str_impl(m, s);
}

static void md_putc_inline(MdCtx *m, int c)
{
	md_emit_char(m, c);
}

static void md_lnbuf_emit_line_finish(MdCtx *m)
{
	char trimbuf[MD_STAMP_TRIM_MAX];
	size_t trimlen;
	int trc;

	if (m->lnlen == 0) {
		fputc('\n', m->out);
		m->stamp_prev_valid = 0;
		return;
	}

	m->lnbuf[m->lnlen] = '\0';
	trc = md_line_trim_copy(m->lnbuf, m->lnlen, trimbuf, sizeof(trimbuf), &trimlen);
	if (trc == 0
	    && trimlen > 0 && m->stamp_prev_valid
	    && md_line_is_post_stamp(trimbuf, trimlen)
	    && gad_str_eq_ci(trimbuf, m->stamp_prev)) {
		m->lnlen = 0;
		return;
	}

	fwrite(m->lnbuf, 1, m->lnlen, m->out);
	fputc('\n', m->out);
	m->lnlen = 0;

	if (trc == 0 && trimlen > 0 && md_line_is_post_stamp(trimbuf, trimlen)
	    && trimlen + (size_t)1 <= sizeof(m->stamp_prev)) {
		memcpy(m->stamp_prev, trimbuf, trimlen);
		m->stamp_prev[trimlen] = '\0';
		m->stamp_prev_valid = 1;
	} else
		m->stamp_prev_valid = 0;
}

static int heading_level_from_tname(const char *tname)
{
	if (gad_str_eq_ci(tname, "h1"))
		return 1;
	if (gad_str_eq_ci(tname, "h2"))
		return 2;
	if (gad_str_eq_ci(tname, "h3"))
		return 3;
	if (gad_str_eq_ci(tname, "h4"))
		return 4;
	if (gad_str_eq_ci(tname, "h5"))
		return 5;
	if (gad_str_eq_ci(tname, "h6"))
		return 6;
	return 0;
}

void md_open_block(MdCtx *m, const char *tname, const char *tag)
{
	char hbuf[ATTRBUF];
	int lvl = heading_level_from_tname(tname);

	if (lvl) {
		size_t i;

		md_putc_inline(m, '\n');
		for (i = (size_t)lvl; i > 0; i--)
			md_putc_inline(m, '#');
		md_putc_inline(m, ' ');
		m->heading = lvl;
		return;
	}
	if (gad_str_eq_ci(tname, "p"))
		return;
	if (gad_str_eq_ci(tname, "blockquote")) {
		m->bq++;
		md_puts(m, "\n\n> ");
		return;
	}
	if (gad_str_eq_ci(tname, "ul")) {
		if (m->list_n < MD_LIST_MAX) {
			m->list_ty[m->list_n] = 0;
			m->list_num[m->list_n] = 0;
			m->list_n++;
		}
		return;
	}
	if (gad_str_eq_ci(tname, "ol")) {
		if (m->list_n < MD_LIST_MAX) {
			m->list_ty[m->list_n] = 1;
			m->list_num[m->list_n] = 1;
			m->list_n++;
		}
		return;
	}
	if (gad_str_eq_ci(tname, "li")) {
		int i, depth = m->list_n - 1;
		char numbuf[32];

		if (depth < 0)
			depth = 0;
		md_putc_inline(m, '\n');
		for (i = 0; i < depth; i++)
			md_puts(m, "  ");
		if (m->enabled && m->list_n > 0 && m->list_ty[m->list_n - 1]) {
			int n = m->list_num[m->list_n - 1]++;

			snprintf(numbuf, sizeof(numbuf), "%d. ", n);
			md_puts(m, numbuf);
		} else
			md_puts(m, "- ");
		return;
	}
	if (gad_str_eq_ci(tname, "pre")) {
		if (!m->pre) {
			md_lnbuf_partial_flush(m);
			md_puts(m, "\n\n```\n");
			m->pre = 1;
		}
		return;
	}
	if (gad_str_eq_ci(tname, "code")) {
		if (m->pre)
			return;
		m->code_inline++;
		md_putc_inline(m, '`');
		return;
	}
	if (gad_str_eq_ci(tname, "strong") || gad_str_eq_ci(tname, "b")) {
		m->strong++;
		md_puts(m, "**");
		return;
	}
	if (gad_str_eq_ci(tname, "em") || gad_str_eq_ci(tname, "i")) {
		m->em++;
		md_puts(m, "*");
		return;
	}
	if (gad_str_eq_ci(tname, "a")) {
		/* Keep only readable link text; suppress markdown URL output. */
		(void)hbuf;
		(void)tag;
		return;
	}
}

void md_close_block(MdCtx *m, const char *tname)
{
	int lvl = heading_level_from_tname(tname);

	if (lvl && m->heading == lvl) {
		md_puts(m, "\n\n");
		m->heading = 0;
		return;
	}
	if (gad_str_eq_ci(tname, "p")) {
		md_puts(m, "\n\n");
		return;
	}
	if (gad_str_eq_ci(tname, "blockquote")) {
		if (m->bq > 0)
			m->bq--;
		md_putc_inline(m, '\n');
		return;
	}
	if (gad_str_eq_ci(tname, "ul") || gad_str_eq_ci(tname, "ol")) {
		if (m->list_n > 0)
			m->list_n--;
		md_putc_inline(m, '\n');
		return;
	}
	if (gad_str_eq_ci(tname, "pre")) {
		if (m->pre) {
			m->pre = 0;
			md_puts(m, "\n```\n\n");
		}
		return;
	}
	if (gad_str_eq_ci(tname, "code")) {
		if (m->pre)
			return;
		if (m->code_inline > 0) {
			m->code_inline--;
			md_putc_inline(m, '`');
		}
		return;
	}
	if (gad_str_eq_ci(tname, "strong") || gad_str_eq_ci(tname, "b")) {
		if (m->strong > 0) {
			m->strong--;
			md_puts(m, "**");
		}
		return;
	}
	if (gad_str_eq_ci(tname, "em") || gad_str_eq_ci(tname, "i")) {
		if (m->em > 0) {
			m->em--;
			md_putc_inline(m, '*');
		}
		return;
	}
	if (gad_str_eq_ci(tname, "a")) {
		return;
	}
}

void md_void_tag(MdCtx *m, const char *tname)
{
	if (gad_str_eq_ci(tname, "br")) {
		md_putc_inline(m, '\n');
		return;
	}
	if (gad_str_eq_ci(tname, "hr"))
		md_puts(m, "\n\n---\n\n");
}
