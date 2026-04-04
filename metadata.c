//
// Created by wright on 2/21/26.
//

#include "metadata.h"

static const char* kMetadataKeyStrs[] = {
#ifndef X
#define X(key) \
    #key,
#endif
    X_METADATA_KEYS
#undef X
};

static const char *kMetadataDelimiter = "---";
static const char *kMetadataFieldAssignDelimiter = "=";

i64 metadata_get(Arena *arena, const string_views *file_lines, MetadataMap *out_map) {
    i64 end_metadata_line_idx = -1;

    i32 meta_delim_count = 0;

    ARRAY_FOR(line, file_lines) {
        if (strncmp(line->data, kMetadataDelimiter, strlen(kMetadataDelimiter)) == 0) {
            meta_delim_count++;
        } else {
            if (meta_delim_count == 1) {
                u64 delim_len = strlen(kMetadataFieldAssignDelimiter);
                const char* assign_delim = strstr(line->data, kMetadataFieldAssignDelimiter);
                if (assign_delim) {
                    string_view key = {
                        line->data,
                        assign_delim - line->data
                    };
                    string_view val = {
                        assign_delim + delim_len,
                        (line->data + line->len + 1) - (assign_delim + delim_len)
                    };

                    str_strip(&key);
                    str_strip(&val);

                    string key_str = str_make_view(arena, &key);
                    string val_str = str_make_view(arena, &val);

                    HASHMAP_PUT(out_map, &key_str.data, &val_str);
                }
            } else if (meta_delim_count >= 2) {
                end_metadata_line_idx = line - file_lines->data;
                break;
            }
        }
    }

    return end_metadata_line_idx;
}

string metadata_key_to_str(Arena* arena, MetadataKey key) {
    const char* key_str = kMetadataKeyStrs[key];

    string lower_key_str = str_make(arena, "%s", key_str);
    str_to_lower(&lower_key_str);

    return lower_key_str;
}
