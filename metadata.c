//
// Created by wright on 2/21/26.
//

#include "metadata.h"

static const char *kMetadataDelimiter = "---";
static const char *kMetadataFieldAssignDelimiter = "=";

bool metadata_get(Arena *arena, const strings *file_lines, MetadataMap *out_map) {
    bool found = false;

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

                    str_view_strip(&key);
                    str_view_strip(&val);
                }
            } else if (meta_delim_count >= 2) {
                found = true;
                break;
            }
        }
    }

    return found;
}
