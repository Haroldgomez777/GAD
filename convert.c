#include "convert.h"
#include "gad_config.h"
#include "html_scan.h"
#include "html_sniff.h"
#include "html_tag.h"
#include "markdown.h"
#include "gad_util.h"

#include <ctype.h>
#include <stdio.h>

void gad_emit_visible_text(FILE *in, FILE *out, int main_mode, int markdown)
{
	int c;
	char tag[TAGBUF];
	int is_style;
	size_t n;
	enum html_root_strat strat = HTML_RS_FULLDOC;
	char deep_tag[TNAMELEN] = "div";
	int in_content = !main_mode;
	int wait_root = 0;
	int root_depth = 0;
	int skip_region = 0;
	int pending_space = 0;
	int last_text_visible = 0;
	char inner_tags[INNERMAX][TNAMELEN];
	int inner_n = 0;
	MdCtx md;

	md_init(&md, out, markdown);

	if (main_mode) {
		html_sniff_root_strategy(in, &strat, deep_tag);
		if (strat == HTML_RS_FULLDOC)
			in_content = 1;
		else
			wait_root = 1;
	}

	while ((c = fgetc(in)) != EOF) {
		if (c != '<') {
			int emit = 1;

			if (main_mode && !in_content)
				emit = 0;
			if (skip_region)
				emit = 0;
			if (!emit)
				continue;
			if (isspace((unsigned char)c)) {
				if (last_text_visible)
					pending_space = 1;
				continue;
			}
			if (pending_space) {
				if (markdown)
					md_emit_char(&md, ' ');
				else
					fputc(' ', out);
				pending_space = 0;
			}
			if (markdown)
				md_emit_char(&md, c);
			else
				fputc(c, out);
			last_text_visible = 1;
			continue;
		}
		c = fgetc(in);
		if (c == EOF)
			break;
		if (c == '!') {
			int a = fgetc(in);
			int b = fgetc(in);

			if (a == '-' && b == '-') {
				html_skip_comment_after_open(in);
				continue;
			}
			n = 0;
			tag[n++] = '<';
			tag[n++] = '!';
			tag[n++] = (char)a;
			tag[n++] = (char)b;
			if (html_append_until_gt_quoted(in, tag, &n, sizeof(tag)) == EOF)
				break;
			continue;
		}
		if (c == '?') {
			n = 0;
			tag[n++] = '<';
			tag[n++] = '?';
			if (html_append_until_gt_quoted(in, tag, &n, sizeof(tag)) == EOF)
				break;
			continue;
		}
		if (ungetc(c, in) == EOF)
			break;
		if (html_read_until_tag_end(in, tag, sizeof(tag)) == EOF)
			break;
		is_style = 0;
		if (html_is_open_head(tag)) {
			if (html_skip_embedded_until_close(in, 2) == EOF)
				break;
			continue;
		}
		if (html_is_open_script_or_style(tag, &is_style)) {
			if (html_skip_embedded_until_close(in, is_style ? 1 : 0) == EOF)
				break;
			continue;
		}
		{
			int is_close = html_tag_is_closing(tag);
			char tname[TNAMELEN];

			html_parse_tag_name(tag, is_close, tname, sizeof(tname));

			if (skip_region) {
				if (is_close) {
					if (inner_n > 0
					    && gad_str_eq_ci(inner_tags[inner_n - 1], tname)) {
						inner_n--;
						if (inner_n == 0)
							skip_region = 0;
					}
				} else if (!html_tag_void_or_selfclose(tag, tname)
				    && inner_n < INNERMAX) {
					size_t j;

					for (j = 0; j < TNAMELEN - 1 && tname[j]; j++)
						inner_tags[inner_n][j] = tname[j];
					inner_tags[inner_n][j] = '\0';
					inner_n++;
				}
				continue;
			}

			if (!is_close && html_generic_skip_subtree(tname, tag)) {
				if (!html_tag_void_or_selfclose(tag, tname)) {
					size_t j;

					skip_region = 1;
					if (inner_n < INNERMAX) {
						for (j = 0; j < TNAMELEN - 1 && tname[j]; j++)
							inner_tags[inner_n][j] = tname[j];
						inner_tags[inner_n][j] = '\0';
						inner_n++;
					}
				}
				continue;
			}

			if (main_mode) {

				if (is_close) {
					if (in_content && root_depth > 0
					    && html_root_closer_matches_strat(strat, tname, deep_tag)) {
						root_depth--;
						if (root_depth == 0)
							in_content = 0;
						continue;
					}
					if (markdown && md_active_for_tag(main_mode, in_content,
					    skip_region, wait_root))
						md_close_block(&md, tname);
					continue;
				}
				if (wait_root) {
					if (html_root_opener_matches_strat(strat, tname, tag, deep_tag)) {
						wait_root = 0;
						in_content = 1;
						root_depth = 1;
					}
					continue;
				}
				if (!in_content)
					continue;
				if (html_tag_void_or_selfclose(tag, tname)) {
					if (markdown && md_active_for_tag(main_mode, in_content,
					    skip_region, wait_root))
						md_void_tag(&md, tname);
					continue;
				}
				if (root_depth > 0 && strat != HTML_RS_FULLDOC) {
					if (strat == HTML_RS_MAIN && gad_str_eq_ci(tname, "main"))
						root_depth++;
					else if (strat == HTML_RS_ARTICLE && gad_str_eq_ci(tname, "article"))
						root_depth++;
					else if (strat == HTML_RS_BODY && gad_str_eq_ci(tname, "body"))
						root_depth++;
					else if ((strat == HTML_RS_ROLEMAIN || strat == HTML_RS_ITEMBODY)
					    && gad_str_eq_ci(tname, deep_tag))
						root_depth++;
				}
				if (markdown && md_active_for_tag(main_mode, in_content, skip_region,
				    wait_root))
					md_open_block(&md, tname, tag);
				continue;
			} else if (markdown) {
				if (is_close)
					md_close_block(&md, tname);
				else if (html_tag_void_or_selfclose(tag, tname))
					md_void_tag(&md, tname);
				else
					md_open_block(&md, tname, tag);
			}
		}
	}
}
