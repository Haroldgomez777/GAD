#ifndef HTML_SNIFF_H
#define HTML_SNIFF_H

#include "gad_config.h"
#include <stdio.h>

enum html_root_strat {
	HTML_RS_FULLDOC,
	HTML_RS_MAIN,
	HTML_RS_ARTICLE,
	HTML_RS_ROLEMAIN,
	HTML_RS_ITEMBODY,
	HTML_RS_BODY
};

void html_sniff_root_strategy(FILE *fp, enum html_root_strat *strat, char deep_tag[TNAMELEN]);
int html_root_opener_matches_strat(enum html_root_strat strat, const char *tname,
    const char *tag, const char *deep_tag);
int html_root_closer_matches_strat(enum html_root_strat strat, const char *tname,
    const char *deep_tag);

#endif
