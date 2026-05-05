#ifndef MARKDOWN_H
#define MARKDOWN_H

#include "gad_config.h"
#include <stdio.h>

typedef struct MdCtx {
	FILE *out;
	int enabled;
	int in_anchor;
	char href[ATTRBUF];
	int strong;
	int em;
	int heading;
	int pre;
	int code_inline;
	int list_ty[MD_LIST_MAX];
	int list_num[MD_LIST_MAX];
	int list_n;
	int bq;
} MdCtx;

void md_init(MdCtx *m, FILE *out, int enabled);
int md_active_for_tag(int main_mode, int in_content, int skip_region, int wait_root);
void md_open_block(MdCtx *m, const char *tname, const char *tag);
void md_close_block(MdCtx *m, const char *tname);
void md_void_tag(MdCtx *m, const char *tname);

#endif
