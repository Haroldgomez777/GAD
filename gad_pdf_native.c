#define _DEFAULT_SOURCE 1

#include "gad_pdf_native.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	PDF_FONT_SIZE = 10,
	PDF_LINE_LEADING = 12,
	PAGE_H = 842,
	MARGIN_PT = 72,
	MAX_COL_CP = 78,
	MAX_TEXT_BYTES = 8 * 1024 * 1024,
	MAX_LINES = 200000,
};

static size_t lines_per_page_count(void)
{
	size_t usable = (size_t)PAGE_H - 2u * (size_t)MARGIN_PT;
	size_t n = usable / (size_t)PDF_LINE_LEADING;

	return n ? n : 1u;
}

struct linebuf {
	char **lines;
	size_t n;
	size_t cap;
};

static void linebuf_free(struct linebuf *lb)
{
	size_t i;

	for (i = 0; i < lb->n; i++)
		free(lb->lines[i]);
	free(lb->lines);
	lb->lines = NULL;
	lb->n = lb->cap = 0;
}

static int linebuf_push(struct linebuf *lb, char *owned_line)
{
	if (lb->n >= MAX_LINES) {
		free(owned_line);
		return -1;
	}
	if (lb->n >= lb->cap) {
		size_t nc = lb->cap ? lb->cap * 2 : 256;
		char **nq = realloc(lb->lines, nc * sizeof(*nq));

		if (!nq) {
			free(owned_line);
			return -1;
		}
		lb->lines = nq;
		lb->cap = nc;
	}
	lb->lines[lb->n++] = owned_line;
	return 0;
}

static const char *utf8_next(const char *s, unsigned *cp)
{
	unsigned char c;

	c = (unsigned char)*s;
	if (c == 0) {
		*cp = 0;
		return s;
	}
	if (c < 0x80u) {
		*cp = c;
		return s + 1;
	}
	if ((c >> 5) == 6) {
		if (s[1] == 0 || ((unsigned char)s[1] & 0xc0u) != 0x80u) {
			*cp = 0xfffd;
			return s + 1;
		}
		*cp = (((unsigned)c & 0x1fu) << 6) | ((unsigned char)s[1] & 0x3fu);
		return s + 2;
	}
	if ((c >> 4) == 14) {
		if (s[1] == 0 || s[2] == 0
		    || ((unsigned char)s[1] & 0xc0u) != 0x80u
		    || ((unsigned char)s[2] & 0xc0u) != 0x80u) {
			*cp = 0xfffd;
			return s + 1;
		}
		*cp = (((unsigned)c & 0x0fu) << 12)
		    | (((unsigned char)s[1] & 0x3fu) << 6)
		    | ((unsigned char)s[2] & 0x3fu);
		return s + 3;
	}
	if ((c >> 3) == 30) {
		if (s[1] == 0 || s[2] == 0 || s[3] == 0
		    || ((unsigned char)s[1] & 0xc0u) != 0x80u
		    || ((unsigned char)s[2] & 0xc0u) != 0x80u
		    || ((unsigned char)s[3] & 0xc0u) != 0x80u) {
			*cp = 0xfffd;
			return s + 1;
		}
		*cp = (((unsigned)c & 7u) << 18) | (((unsigned char)s[1] & 0x3fu) << 12)
		    | (((unsigned char)s[2] & 0x3fu) << 6) | ((unsigned char)s[3] & 0x3fu);
		if (*cp > 0x10ffffu)
			*cp = 0xfffd;
		return s + 4;
	}
	*cp = 0xfffd;
	return s + 1;
}

static unsigned cp_to_pdf_byte(unsigned cp)
{
	if (cp == '\t' || cp == '\v')
		return (unsigned)' ';
	if (cp < 32u && cp != '\n' && cp != '\r' && cp != 0)
		return 0;
	if (cp <= 255u)
		return cp;
	return (unsigned)'?';
}

static size_t utf8_cp_len(const char *start, const char *end)
{
	size_t cnt = 0;

	while (start < end && *start) {
		unsigned cp;

		start = utf8_next(start, &cp);
		if (cp != 0)
			cnt++;
	}
	return cnt;
}

struct grow {
	char *p;
	size_t len;
	size_t cap;
};

static void grow_free(struct grow *g)
{
	free(g->p);
	g->p = NULL;
	g->len = g->cap = 0;
}

static int grow_app(struct grow *g, const void *data, size_t n)
{
	size_t need = g->len + n;

	if (need > g->cap) {
		size_t nc = g->cap ? g->cap * 2 : 1024;

		while (nc < need)
			nc *= 2;
		{
			char *q = realloc(g->p, nc);

			if (!q)
				return -1;
			g->p = q;
			g->cap = nc;
		}
	}
	memcpy(g->p + g->len, data, n);
	g->len = need;
	return 0;
}

static int grow_appf(struct grow *g, const char *fmt, ...)
{
	va_list ap;
	int n;
	char *tmp;

	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (n < 0)
		return -1;
	while (g->len + (size_t)n + 1u > g->cap) {
		size_t nc = g->cap ? g->cap * 2 : 1024;

		if (nc < g->len + (size_t)n + 1u)
			nc = g->len + (size_t)n + 1u;
		{
			char *q = realloc(g->p, nc);

			if (!q)
				return -1;
			g->p = q;
			g->cap = nc;
		}
	}
	tmp = g->p + g->len;
	va_start(ap, fmt);
	vsnprintf(tmp, g->cap - g->len, fmt, ap);
	va_end(ap);
	g->len += (size_t)n;
	return 0;
}

static int append_pdf_literal_from_utf8(struct grow *g, const char *utf8)
{
	unsigned cp;

	if (grow_app(g, "(", 1) != 0)
		return -1;
	while (*utf8) {
		utf8 = utf8_next(utf8, &cp);
		if (cp == 0)
			break;
		{
			unsigned b = cp_to_pdf_byte(cp);

			if (b == 0)
				continue;
			if (b == (unsigned)'\\') {
				if (grow_app(g, "\\\\", 2) != 0)
					return -1;
			} else if (b == '(') {
				if (grow_app(g, "\\(", 2) != 0)
					return -1;
			} else if (b == ')') {
				if (grow_app(g, "\\)", 2) != 0)
					return -1;
			} else if (b < 128u) {
				char c = (char)b;

				if (grow_app(g, &c, 1) != 0)
					return -1;
			} else {
				unsigned char uc = (unsigned char)b;

				if (grow_app(g, &uc, 1) != 0)
					return -1;
			}
		}
	}
	return grow_app(g, ")", 1);
}

static char *dup_range(const char *a, const char *b)
{
	size_t n;
	char *s;

	if (b < a)
		return NULL;
	n = (size_t)(b - a);
	s = malloc(n + 1u);
	if (!s)
		return NULL;
	memcpy(s, a, n);
	s[n] = '\0';
	return s;
}

/* Push hard-wrapped segments from [w0,w1) when one word exceeds MAX_COL_CP. */
static int push_long_word_chunks(struct linebuf *lb, const char *w0,
    const char *w1)
{
	const char *cur = w0;

	while (cur < w1) {
		const char *seg = cur;
		size_t cols = 0;

		while (cur < w1) {
			unsigned cp;
			const char *nxt = utf8_next(cur, &cp);

			if (nxt == cur)
				return -1;
			if (cols >= (size_t)MAX_COL_CP)
				break;
			cols++;
			cur = nxt;
		}
		if (cur == seg)
			return -1;
		{
			char *ln = dup_range(seg, cur);

			if (!ln || linebuf_push(lb, ln) != 0)
				return -1;
		}
	}
	return 0;
}

static int wrap_physical_line(struct linebuf *lb, const char *line,
    const char *line_end)
{
	const char *sol = line;
	const char *tok = line;
	size_t cols = 0;
	const char *last_byte = line;

	while (tok < line_end && *tok) {
		while (tok < line_end && (*tok == ' ' || *tok == '\t'))
			tok++;
		if (tok >= line_end || *tok == 0)
			break;
		{
			const char *w0 = tok;

			while (tok < line_end && *tok != ' ' && *tok != '\t') {
				unsigned cp;

				tok = utf8_next(tok, &cp);
			}
			{
				size_t wcp = utf8_cp_len(w0, tok);

				if (wcp == 0)
					continue;
				if (wcp > (size_t)MAX_COL_CP) {
					if (cols > 0) {
						char *ln = dup_range(sol, last_byte);

						if (!ln || linebuf_push(lb, ln) != 0)
							return -1;
					}
					if (push_long_word_chunks(lb, w0, tok) != 0)
						return -1;
					sol = tok;
					last_byte = tok;
					cols = 0;
					continue;
				}
				if (cols == 0) {
					sol = w0;
					last_byte = tok;
					cols = wcp;
					continue;
				}
				if (cols + 1u + wcp > (size_t)MAX_COL_CP) {
					char *ln = dup_range(sol, last_byte);

					if (!ln || linebuf_push(lb, ln) != 0)
						return -1;
					sol = w0;
					last_byte = tok;
					cols = wcp;
				} else {
					last_byte = tok;
					cols += 1u + wcp;
				}
			}
		}
	}
	{
		char *ln = dup_range(sol, last_byte);

		if (!ln)
			return -1;
		if (ln[0] == 0) {
			free(ln);
			return 0;
		}
		return linebuf_push(lb, ln);
	}
}

static int text_to_lines(const char *buf, struct linebuf *lb)
{
	const char *p = buf;
	const char *sol = buf;

	for (;;) {
		unsigned char c = (unsigned char)*p;

		if (c == 0) {
			if (wrap_physical_line(lb, sol, p) != 0)
				return -1;
			break;
		}
		if (c == '\r' && p[1] == '\n') {
			if (wrap_physical_line(lb, sol, p) != 0)
				return -1;
			p += 2;
			sol = p;
			continue;
		}
		if (c == '\n') {
			if (wrap_physical_line(lb, sol, p) != 0)
				return -1;
			p++;
			sol = p;
			continue;
		}
		p++;
	}
	return 0;
}

static int build_page_stream(struct grow *g, char **lines, size_t ln0,
    size_t ln1)
{
	double x = (double)MARGIN_PT;
	double y0 = (double)PAGE_H - (double)MARGIN_PT - (double)PDF_FONT_SIZE;
	size_t li;

	if (grow_app(g, "BT\n", 3) != 0)
		return -1;
	if (grow_appf(g, "/F1 %d Tf\n%u TL\n%.6f %.6f Td\n", PDF_FONT_SIZE,
		(unsigned)PDF_LINE_LEADING, x, y0)
	    != 0)
		return -1;

	for (li = ln0; li < ln1; li++) {
		const char *s = lines[li] ? lines[li] : "";

		if (!s[0]) {
			if (grow_app(g, "() Tj\n", 6) != 0)
				return -1;
		} else {
			if (append_pdf_literal_from_utf8(g, s) != 0)
				return -1;
			if (grow_app(g, " Tj\n", 4) != 0)
				return -1;
		}
		if (li + 1 < ln1 && grow_app(g, "T*\n", 3) != 0)
			return -1;
	}
	return grow_app(g, "ET\n", 3);
}

static int write_stream_obj(FILE *fp, long *offsets, int id, const char *dat,
    size_t len)
{
	long pos = ftell(fp);

	offsets[id] = pos;
	fprintf(fp,
	    "%d 0 obj\n<< /Length %zu >>\nstream\n",
	    id, len);
	if (len && fwrite(dat, 1, len, fp) != len)
		return -1;
	fputs("endstream\nendobj\n", fp);
	return 0;
}

int gad_try_emit_pdf_native(const char *textpath, const char *outpdf)
{
	char *raw = NULL;
	struct linebuf lb = { NULL, 0, 0 };
	size_t lpp = lines_per_page_count();
	size_t npages;
	size_t pi;
	FILE *fp = NULL;
	long *offsets = NULL;
	int objmax;
	struct grow pg = { NULL, 0, 0 };
	long xref_pos;
	size_t nlines;

	if (!textpath || !outpdf || !outpdf[0]) {
		errno = EINVAL;
		return -1;
	}
	{
		FILE *rf = fopen(textpath, "rb");
		long sz;

		if (!rf)
			return -1;
		if (fseek(rf, 0, SEEK_END) != 0) {
			fclose(rf);
			return -1;
		}
		sz = ftell(rf);
		if (sz < 0) {
			fclose(rf);
			return -1;
		}
		if (sz > (long)MAX_TEXT_BYTES) {
			fclose(rf);
			errno = EFBIG;
			return -1;
		}
		rewind(rf);
		raw = malloc((size_t)sz + 1u);
		if (!raw) {
			fclose(rf);
			return -1;
		}
		if (sz > 0 && fread(raw, (size_t)sz, 1u, rf) != 1u) {
			free(raw);
			fclose(rf);
			return -1;
		}
		raw[sz] = '\0';
		fclose(rf);
	}

	if (text_to_lines(raw, &lb) != 0)
		goto fail;
	free(raw);
	raw = NULL;

	if (lb.n == 0) {
		char *el = strdup("");

		if (!el || linebuf_push(&lb, el) != 0) {
			free(el);
			goto fail;
		}
	}

	nlines = lb.n;
	npages = (nlines + lpp - 1u) / lpp;
	if (npages == 0)
		npages = 1u;

	objmax = 3 + (int)(2u * npages);
	offsets = calloc((size_t)objmax + 1u, sizeof(*offsets));
	if (!offsets)
		goto fail;

	fp = fopen(outpdf, "wb");
	if (!fp)
		goto fail;

	fputs("%PDF-1.4\n", fp);

	offsets[1] = ftell(fp);
	fprintf(fp,
	    "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

	offsets[2] = ftell(fp);
	fprintf(fp, "2 0 obj\n<< /Type /Pages /Count %zu /Kids [",
	    npages);
	for (pi = 0; pi < npages; pi++)
		fprintf(fp, "%d 0 R ", 4 + (int)pi);
	fprintf(fp, "] >>\nendobj\n");

	offsets[3] = ftell(fp);
	fprintf(fp,
	    "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n");

	for (pi = 0; pi < npages; pi++) {
		int page_id = 4 + (int)pi;
		int content_id = 4 + (int)npages + (int)pi;
		size_t ln0 = pi * lpp;
		size_t ln1 = ln0 + lpp;

		if (ln1 > nlines)
			ln1 = nlines;

		grow_free(&pg);
		pg.p = NULL;
		pg.len = pg.cap = 0;
		if (build_page_stream(&pg, lb.lines, ln0, ln1) != 0)
			goto fail;

		offsets[page_id] = ftell(fp);
		fprintf(fp,
		    "%d 0 obj\n<< /Type /Page /Parent 2 0 R "
		    "/MediaBox [0 0 595 842] /Contents %d 0 R "
		    "/Resources << /Font << /F1 3 0 R >> >> >>\nendobj\n",
		    page_id,
		    content_id);
		if (write_stream_obj(fp, offsets, content_id,
			pg.p ? pg.p : "",
			pg.len) != 0)
			goto fail;
	}

	xref_pos = ftell(fp);
	fprintf(fp, "xref\n0 %d\n", objmax + 1);
	fprintf(fp, "0000000000 65535 f \n");
	{
		int oid;

		for (oid = 1; oid <= objmax; oid++)
			fprintf(fp, "%010ld 00000 n \n", offsets[oid]);
	}
	fprintf(fp,
	    "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
	    objmax + 1,
	    xref_pos);

	if (ferror(fp)) {
		remove(outpdf);
		goto fail;
	}
	fclose(fp);
	fp = NULL;
	grow_free(&pg);
	free(offsets);
	offsets = NULL;
	linebuf_free(&lb);
	return 0;

fail:
	grow_free(&pg);
	linebuf_free(&lb);
	free(raw);
	free(offsets);
	if (fp) {
		fclose(fp);
		remove(outpdf);
	}
	return -1;
}