//
// Created by wright on 2/21/26.
//

#include "body.h"

#include "altcore/defer.h"

typedef enum ARTICLE_TOKEN_TYPE_E {
#ifndef X_ARTICLE_TOKEN_TYPES
#define X_ARTICLE_TOKEN_TYPES \
    X(NONE) \
    X(HEADING) \
    X(PARAGRAPH) \
    X(BLOCKQUOTE) \
    X(UNORDERED_LIST) \
    X(ORDERED_LIST) \
    X(LABEL) \
    X(COUNT)
#endif

#ifndef X
#define X(type) \
    ARTICLE_TOKEN_TYPE_##type,
#endif
    X_ARTICLE_TOKEN_TYPES
#undef X
} ArticleTokenType;

typedef struct HEADING_TOKEN_DATA_T {
    i32 level;
    string text;
} HeadingTokenData;

typedef struct PARAGRAPH_TOKEN_DATA_T {
} ParagraphTokenData;

typedef struct LABEL_TOKEN_DATA_T {
    string name;
    i64 ref_count;
} LabelTokenData;

typedef enum TOKEN_PAREN_E {
#ifndef X_TOKEN_PARENS
#define X_TOKEN_PARENS \
    X(NONE) \
    X(OPEN) \
    X(CLOSE) \
    X(COUNT)
#endif

#ifndef X
#define X(paren) \
    TOKEN_PAREN_##paren,
#endif
    X_TOKEN_PARENS
#undef X
} TokenParen;

typedef struct ARTICLE_TOKEN_T {
    TokenParen paren;
    ArticleTokenType type;

    union {
        HeadingTokenData heading;
        ParagraphTokenData paragraph;
        LabelTokenData label;
    } data;
} ArticleToken;

typedef struct ARTICLE_TOKENS_T {
    ARRAY_FIELDS(ArticleToken)
} ArticleTokens;

typedef struct LABELS_MAP_T {
    HASHMAP_FIELDS(const char*, bool)
} LabelsMap;

typedef struct METABLOCK_RANGE_T {
    i64 start_c_idx, end_c_idx;
} MetablockRange;

static const char *kMetablockStartDelimiter = "{{";
static const char *kMetablockEndDelimiter = "}}";
static const char *kMetablockLabelKey = "label";

static MetablockRange metablock_find_range(Arena *arena, const string_view *str_view) {
    MetablockRange range = {-1, -1};

    const i64 arena_start_offset = arena->offset;

    string str = str_view_make(arena, str_view);

    u64 delim_total_len = strlen(kMetablockStartDelimiter) + strlen(kMetablockEndDelimiter);

    if (str.len <= delim_total_len) {
        // Empty metablock
        return range;
    }

    const char *metablock_start = strstr(str.data, kMetablockStartDelimiter);
    if (!metablock_start) {
        return range;
    }

    range.start_c_idx = metablock_start - str.data;

    const char *metablock_content_start = metablock_start + strlen(kMetablockStartDelimiter);

    const char *metablock_end = strstr(metablock_content_start, kMetablockEndDelimiter);

    if (metablock_end) {
        range.end_c_idx = metablock_end - str.data;
    }

    arena->offset = arena_start_offset;

    return range;
}

static bool label_get_metablock(
    Arena *arena,
    LabelsMap *in_labels_map,
    const string_view *in_label_metablock,
    LabelTokenData *out_label_tk) {
    if (!arena || !out_label_tk) {
        return false;
    }

    const char *start_delim = strstr(in_label_metablock->data, kMetablockStartDelimiter);
    if (!start_delim) {
        return false;
    }

    u64 start_delim_len = strlen(kMetablockStartDelimiter);

    const char *end_delim = strstr(start_delim + start_delim_len, kMetablockEndDelimiter);
    if (!end_delim) {
        return false;
    }

    string_view contents = {start_delim + start_delim_len, (end_delim) - (start_delim + start_delim_len)};

    str_view_strip(&contents);

    string content_str = str_view_make(arena, &contents);

    strings terms = str_split(arena, &content_str, " ");

    if (terms.len < 2) {
        return false;
    }

    if (strncmp(terms.data[0].data, kMetablockLabelKey, strlen(kMetablockLabelKey)) != 0) {
        return false;
    }

    bool exists = HASHMAP_GET_VAL(in_labels_map, &terms.data[1].data);

    if (exists) {
        return false;
    }

    out_label_tk->name = terms.data[1];

    return true;
}

void body_to_html(
    Arena *arena,
    const MetadataMap *metadata,
    const strings *file_lines,
    i64 body_start_line_idx,
    string *out_html
) {
    if (body_start_line_idx < 0 || body_start_line_idx >= file_lines->len) {
        return;
    }

    ArticleTokens tks = {arena};
    ARRAY_MAKE(&tks);

    LabelsMap existing_labels = {HASHMAP_TYPE_STR_KEY};
    bool default_label_val = false;
    HASHMAP_MAKE(&existing_labels, &default_label_val);

    i64 current_open_tk_idx = -1;

    for (i64 line_idx = body_start_line_idx; line_idx < file_lines->len; line_idx++) {
        string *line = &file_lines->data[line_idx];

        string_view line_view = {line->data, line->len};

        if (current_open_tk_idx < 0) {
            str_view_strip(&line_view);

            if (line_view.len > 0) {
                switch (line_view.data[0]) {
                    case '#': {
                        // Heading
                        ArticleTokenType tk_type = ARTICLE_TOKEN_TYPE_HEADING;
                        ArticleToken heading_open_tk = {
                            TOKEN_PAREN_OPEN,
                            tk_type,
                        };

                        i64 text_start_idx;
                        for (text_start_idx = 0; text_start_idx < line_view.len; text_start_idx++) {
                            if (line_view.data[text_start_idx] != '#') {
                                break;
                            }
                        }

                        if (text_start_idx >= line_view.len) {
                            break;
                        }

                        heading_open_tk.data.heading.level = (i32) text_start_idx;

                        bool label_present = false;
                        LabelTokenData label_tk_data = {};

                        if (line_view.data[text_start_idx] == kMetablockStartDelimiter[0]) {
                            // label
                            MetablockRange label_range = metablock_find_range(arena, &line_view);
                            if (label_range.start_c_idx >= 0 && label_range.end_c_idx >= 0) {
                                string_view metablock_view = {
                                    line_view.data + label_range.start_c_idx,
                                    label_range.end_c_idx - label_range.start_c_idx
                                };

                                label_present = label_get_metablock(arena, &existing_labels, &metablock_view, &label_tk_data);
                                if (label_present) {
                                    text_start_idx = (i32) (label_range.end_c_idx + strlen(kMetablockEndDelimiter));
                                }
                            }
                        }

                        str_view_advance(&line_view, text_start_idx);
                        str_view_strip(&line_view);
                        heading_open_tk.data.heading.text = str_view_make(arena, &line_view);
                        ARRAY_PUSH(&tks, &heading_open_tk);

                        if (label_present) {
                            ArticleToken label_open_tk = {
                                TOKEN_PAREN_OPEN,
                                ARTICLE_TOKEN_TYPE_LABEL
                            };
                            label_open_tk.data.label = label_tk_data;

                            ARRAY_PUSH(&tks, &label_open_tk);

                            ArticleToken label_close_tk = {
                                TOKEN_PAREN_CLOSE,
                                ARTICLE_TOKEN_TYPE_LABEL
                            };

                            ARRAY_PUSH(&tks, &label_close_tk);
                        }

                        ArticleToken heading_close_tk = {
                            TOKEN_PAREN_CLOSE,
                            tk_type,
                        };

                        ARRAY_PUSH(&tks, &heading_close_tk);

                        break;
                    }
                    case '>': {
                        // Blockquote
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    HASHMAP_FREE(&existing_labels);
}
