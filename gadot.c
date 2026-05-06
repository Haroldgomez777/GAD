/*
 * Convert HTML to plain text or Markdown (visible body text / structure).
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
 * With -M, output is Markdown: headings, lists, links, emphasis, blockquote,
 * horizontal rule, line breaks, and fenced pre blocks (subset; not full HTML).
 *
 * With -P, writes PDF via pandoc, trying several engines (TeX, wkhtmltopdf, …),
 *   then HTML→PDF via weasyprint or wkhtmltopdf. If none are installed, uses a
 *   built-in plain-text PDF (no extra packages; HTML is converted to text first).
 *
 * Build: make   (or list of .c files including gad_input.c)
 * Usage: gad [-m|-w] [-M] [-P] input.html|URL [output]
 *   output default: copy.txt, copy.md if -M, copy.pdf if -P
 */

#include "convert.h"
#include "gad_input.h"
#include "gad_pdf.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

int main(int argc, char const *argv[])
{
	FILE *fp;
	const char *out_path;
	pid_t fetch_child = -1;
	int main_mode = 0;
	int markdown = 0;
	int pdf_mode = 0;
	int ai = 1;

	while (ai < argc && argv[ai][0] == '-' && argv[ai][1] != '\0') {
		if (strcmp(argv[ai], "-m") == 0 || strcmp(argv[ai], "-w") == 0)
			main_mode = 1;
		else if (strcmp(argv[ai], "-M") == 0)
			markdown = 1;
		else if (strcmp(argv[ai], "-P") == 0)
			pdf_mode = 1;
		else {
			fprintf(stderr, "%s: unknown option `%s`\n", argv[0], argv[ai]);
			fprintf(stderr,
				"usage: %s [-m|-w] [-M] [-P] input.html|URL [output]\n"
				"  input: local file or http(s)://… (curl or wget must be in PATH)\n"
				"  -m  Main content: HTML5 landmarks + ARIA (any site)\n"
				"  -w  Same as -m (alias)\n"
				"  -M  Markdown output (headings, lists, links, emphasis, …)\n"
				"  -P  PDF: pandoc/html engines if installed, else built-in text PDF (use -M for richer text)\n"
				"  Default output: copy.txt, copy.md if -M, copy.pdf if -P\n",
				argv[0]);
			return 1;
		}
		ai++;
	}
	if (argc - ai < 1) {
		fprintf(stderr,
			"usage: %s [-m|-w] [-M] [-P] input.html|URL [output]\n"
			"  -m  Main content: HTML5 landmarks + ARIA (any site), not CSS-class rules\n"
			"  -w  Same as -m (alias)\n"
			"  -M  Markdown output\n"
			"  -P  PDF (pandoc/weasyprint/wkhtmltopdf when available, else built-in text PDF)\n"
			"      optional GAD_PDF_ENGINE for pandoc --pdf-engine\n"
			"  input: local file or http(s)://… (needs curl or wget for URLs)\n"
			"  Default output: copy.txt, copy.md if -M, copy.pdf if -P\n",
			argv[0]);
		return 1;
	}

	fp = gad_open_input(argv[ai], &fetch_child);
	if (!fp) {
		if (!gad_is_http_url(argv[ai]))
			perror(argv[ai]);
		return 1;
	}

	if (argc - ai < 2) {
		if (pdf_mode)
			out_path = "copy.pdf";
		else if (markdown)
			out_path = "copy.md";
		else
			out_path = "copy.txt";
	} else
		out_path = argv[ai + 1];

	if (pdf_mode) {
		if (gad_emit_pdf(fp, main_mode, markdown, out_path) != 0) {
			gad_close_input(fp, fetch_child);
			return 1;
		}
	} else {
		FILE *fp2 = fopen(out_path, "wb");
		if (!fp2) {
			perror(out_path);
			gad_close_input(fp, fetch_child);
			return 1;
		}
		gad_emit_visible_text(fp, fp2, main_mode, markdown);
		if (fclose(fp2) != 0) {
			perror(out_path);
			gad_close_input(fp, fetch_child);
			return 1;
		}
	}

	if (gad_close_input(fp, fetch_child) != 0)
		return 1;
	return 0;
}
