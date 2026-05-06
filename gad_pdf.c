#define _DEFAULT_SOURCE 1

#include "gad_pdf.h"
#include "gad_pdf_native.h"
#include "convert.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

static int child_ok(pid_t p)
{
	int st;

	if (p < 0)
		return 0;
	if (waitpid(p, &st, 0) < 0)
		return 0;
	return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/* Hides pandoc/weasyprint noise while probing engines. */
static int spawn_execvp(char *const argv[])
{
	pid_t p = fork();
	int fdnull;

	if (p < 0) {
		perror("gad: fork");
		return 0;
	}
	if (p == 0) {
		fdnull = open("/dev/null", O_WRONLY);
		if (fdnull >= 0) {
			dup2(fdnull, STDERR_FILENO);
			close(fdnull);
		}
		execvp(argv[0], argv);
		_exit(127);
	}
	return child_ok(p);
}

static int try_pandoc_pdf(const char *infile, const char *outpdf, int markdown,
    const char *engine)
{
	char *av[16];
	int i = 0;
	const char *fmt = markdown ? "markdown" : "plain";

	av[i++] = "pandoc";
	av[i++] = "-f";
	av[i++] = (char *)fmt;
	av[i++] = "-o";
	av[i++] = (char *)outpdf;
	if (engine && engine[0]) {
		av[i++] = "--pdf-engine";
		av[i++] = (char *)engine;
	}
	av[i++] = (char *)infile;
	av[i++] = NULL;
	return spawn_execvp(av);
}

static int try_html_intermediate_pdf(const char *infile, const char *outpdf,
    int markdown)
{
	const char *tmpd;
	char html[512];
	int hfd;
	const char *fmt = markdown ? "markdown" : "plain";
	char *phtml[16];

	tmpd = getenv("TMPDIR");
	if (!tmpd || !tmpd[0])
		tmpd = "/tmp";
	snprintf(html, sizeof(html), "%s/gadhXXXXXX.html", tmpd);
	hfd = mkstemps(html, 5);
	if (hfd < 0) {
		perror("gad: mkstemps");
		return 0;
	}
	close(hfd);
	phtml[0] = "pandoc";
	phtml[1] = "-f";
	phtml[2] = (char *)fmt;
	phtml[3] = "--metadata";
	phtml[4] = "title=Document";
	phtml[5] = "-t";
	phtml[6] = "html5";
	phtml[7] = "-s";
	phtml[8] = "-o";
	phtml[9] = html;
	phtml[10] = (char *)infile;
	phtml[11] = NULL;
	if (!spawn_execvp(phtml)) {
		unlink(html);
		return 0;
	}
	{
		char *ww[] = { "weasyprint", html, (char *)outpdf, NULL };

		if (spawn_execvp(ww)) {
			unlink(html);
			return 1;
		}
	}
	{
		char *wk[] = { "wkhtmltopdf", "-q", "--enable-local-file-access", html,
		    (char *)outpdf, NULL };

		if (spawn_execvp(wk)) {
			unlink(html);
			return 1;
		}
	}
	unlink(html);
	return 0;
}

static int run_pandoc_attempts(const char *infile, const char *outpdf, int markdown)
{
	const char *user_eng;
	static const char *engines[] = {
		"pdflatex", "xelatex", "lualatex", "pdfroff", "wkhtmltopdf", "weasyprint",
		NULL
	};
	size_t k;

	user_eng = getenv("GAD_PDF_ENGINE");
	if (user_eng && user_eng[0] && try_pandoc_pdf(infile, outpdf, markdown, user_eng))
		return 1;
	if (try_pandoc_pdf(infile, outpdf, markdown, NULL))
		return 1;
	for (k = 0; engines[k]; k++) {
		if (user_eng && strcmp(engines[k], user_eng) == 0)
			continue;
		if (try_pandoc_pdf(infile, outpdf, markdown, engines[k]))
			return 1;
	}
	return 0;
}

int gad_emit_pdf(FILE *in, int main_mode, int markdown, const char *pdf_path)
{
	const char *tmpd;
	char src[512];
	int fd, sfx;
	FILE *out;
	const char *suf;

	if (!in || !pdf_path || !pdf_path[0]) {
		errno = EINVAL;
		return -1;
	}

	tmpd = getenv("TMPDIR");
	if (!tmpd || !tmpd[0])
		tmpd = "/tmp";
	suf = markdown ? ".md" : ".txt";
	sfx = (int)strlen(suf);
	snprintf(src, sizeof(src), "%s/gadconvXXXXXX%s", tmpd, suf);
	fd = mkstemps(src, sfx);
	if (fd < 0) {
		perror("gad: mkstemps");
		return -1;
	}
	out = fdopen(fd, "wb");
	if (!out) {
		perror("gad: fdopen");
		close(fd);
		unlink(src);
		return -1;
	}
	gad_emit_visible_text(in, out, main_mode, markdown);
	if (fclose(out) != 0) {
		perror("gad: fclose");
		unlink(src);
		return -1;
	}

	if (!run_pandoc_attempts(src, pdf_path, markdown)
	    && !try_html_intermediate_pdf(src, pdf_path, markdown)) {
		if (gad_try_emit_pdf_native(src, pdf_path) == 0) {
			unlink(src);
			return 0;
		}
		fprintf(stderr,
		    "gad: could not produce PDF (external engines failed and built-in text PDF failed). Optional backends:\n"
		    "    Debian/Ubuntu: sudo apt install texlive-latex-base\n"
		    "    or: sudo apt install wkhtmltopdf  /  pip install weasyprint\n"
		    "    or: sudo apt install groff  (provides pdfroff on some releases)\n"
		    "    Or set GAD_PDF_ENGINE to a pandoc-supported engine.\n"
		    "    With no extras, gad falls back to plain-text PDF (Helvetica, Latin-1).\n");
		unlink(src);
		return -1;
	}
	unlink(src);
	return 0;
}
