//
// Created by wright on 2/21/26.
//

#ifndef ARTICLE_HTML_BODY_H
#define ARTICLE_HTML_BODY_H

#include <altcore/strings.h>
#include "metadata.h"

void body_to_html(
    Arena *arena,
    const MetadataMap *metadata,
    const strings *file_lines,
    i64 body_start_line_idx,
    string *out_html
);

#endif //ARTICLE_HTML_BODY_H
