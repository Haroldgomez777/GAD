#include "markdown.h"
#include "html_attr.h"
#include "gad_util.h"

#include <stdio.h>
#include <string.h>


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

static void md_puts(MdCtx *m, const char *s)
{
	if (m->enabled)
		fputs(s, m->out);
}

static void md_putc(MdCtx *m, int c)
{
	if (m->enabled)
		fputc(c, m->out);
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

		md_putc(m, '\n');
		for (i = (size_t)lvl; i > 0; i--)
			md_putc(m, '#');
		md_putc(m, ' ');
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

		if (depth < 0)
			depth = 0;
		md_putc(m, '\n');
		for (i = 0; i < depth; i++)
			md_puts(m, "  ");
		if (m->enabled && m->list_n > 0 && m->list_ty[m->list_n - 1]) {
			int n = m->list_num[m->list_n - 1]++;

			fprintf(m->out, "%d. ", n);
		} else
			md_puts(m, "- ");
		return;
	}
	if (gad_str_eq_ci(tname, "pre")) {
		if (!m->pre) {
			m->pre = 1;
			md_puts(m, "\n\n```\n");
		}
		return;
	}
	if (gad_str_eq_ci(tname, "code")) {
		if (m->pre)
			return;
		m->code_inline++;
		md_putc(m, '`');
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
		html_extract_attr_ci(tag, "href", hbuf, sizeof(hbuf));
		m->in_anchor = 1;
		strncpy(m->href, hbuf, sizeof(m->href) - 1);
		m->href[sizeof(m->href) - 1] = '\0';
		md_putc(m, '[');
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
		md_putc(m, '\n');
		return;
	}
	if (gad_str_eq_ci(tname, "ul") || gad_str_eq_ci(tname, "ol")) {
		if (m->list_n > 0)
			m->list_n--;
		md_putc(m, '\n');
		return;
	}
	if (gad_str_eq_ci(tname, "pre")) {
		if (m->pre) {
			md_puts(m, "\n```\n\n");
			m->pre = 0;
		}
		return;
	}
	if (gad_str_eq_ci(tname, "code")) {
		if (m->pre)
			return;
		if (m->code_inline > 0) {
			m->code_inline--;
			md_putc(m, '`');
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
			md_putc(m, '*');
		}
		return;
	}
	if (gad_str_eq_ci(tname, "a")) {
		if (m->enabled && m->in_anchor) {
			m->in_anchor = 0;
			fputc(']', m->out);
			fputc('(', m->out);
			fputs(m->href[0] ? m->href : "", m->out);
			fputc(')', m->out);
			m->href[0] = '\0';
		}
		return;
	}
}

void md_void_tag(MdCtx *m, const char *tname)
{
	if (gad_str_eq_ci(tname, "br")) {
		md_putc(m, '\n');
		return;
	}
	if (gad_str_eq_ci(tname, "hr"))
		md_puts(m, "\n\n---\n\n");
}
