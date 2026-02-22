//
// Created by wright on 2/21/26.
//

#ifndef ARTICLE_HTML_METADATA_H
#define ARTICLE_HTML_METADATA_H

#include <altcore/hashmap.h>
#include <altcore/strings.h>

typedef struct METADATA_MAP_T {
    HASHMAP_FIELDS(const char*, string)
} MetadataMap;

i64 metadata_get(Arena *arena, const strings *file_lines, MetadataMap *out_map);

#endif //ARTICLE_HTML_METADATA_H
