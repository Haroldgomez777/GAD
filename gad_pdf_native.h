#ifndef GAD_PDF_NATIVE_H
#define GAD_PDF_NATIVE_H

/* Writes a minimal multi-page PDF from UTF-8 plain text using only the PDF
 * standard Helvetica font (no fonts embedded). Requires no external programs.
 * Best-effort: code points outside Latin-1 are replaced with '?'.
 * Returns 0 on success, -1 on failure. */
int gad_try_emit_pdf_native(const char *textpath, const char *outpdf);

#endif
