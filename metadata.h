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

typedef enum METADATA_KEY_E : u32 {
#ifndef X_METADATA_KEYS
#define X_METADATA_KEYS \
    X(TITLE) \
    X(SUBTITLE) \
    X(AUTHOR) \
    X(CREATED) \
    X(MODIFIED) \
    X(REFS) \
    X(DEFAULT_BIBLE) \
    X(COUNT)
#endif
#ifndef X
#define X(key) \
    METADATA_KEY_##key,
#endif
    X_METADATA_KEYS
#undef X
} MetadataKey;

i64 metadata_get(Arena *arena, const string_views *file_lines, MetadataMap *out_map);

string metadata_key_to_str(Arena* arena, MetadataKey key);

#endif //ARTICLE_HTML_METADATA_H
