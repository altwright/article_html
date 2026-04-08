//
// Created by wright on 2/21/26.
//

#include "body.h"

#include <assert.h>
#include "bible.h"
#include <altcore/defer.h>
#include <bibtool_wrapper/library.h>
#include <cwalk.h>

typedef struct METABLOCK_RANGE_T {
    i64 start_c_idx, end_c_idx;
} MetablockRange;

typedef enum ARTICLE_TOKEN_TYPE_E {
#ifndef X_ARTICLE_TOKEN_TYPES
#define X_ARTICLE_TOKEN_TYPES \
    X(NONE) \
    X(HEADING) \
    X(PARAGRAPH) \
    X(REGULAR_TEXT) \
    X(ITALIC_TEXT) \
    X(BOLD_TEXT) \
    X(UNDERLINED_TEXT) \
    X(UNORDERED_LIST) \
    X(ORDERED_LIST) \
    X(LIST_ITEM) \
    X(LABEL) \
    X(LABEL_DISPLAY) \
    X(BIBLE_BLOCK) \
    X(BIBLE_HOVER) \
    X(BIBLE_CITE) \
    X(CITE) \
    X(BLOCK_QUOTE) \
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

typedef struct TEXT_TOKEN_DATA_T {
    i64 start_line_idx;
    i64 start_c_idx;
    string text;
} TextTokenData;

typedef TextTokenData RegularTextTokenData;
typedef TextTokenData ItalicTextTokenData;
typedef TextTokenData BoldTextTokenData;

typedef struct BLOCK_QUOTE_TOKEN_DATA_T {
    string text;
} BlockQuoteTokenData;

typedef struct LABEL_TOKEN_DATA_T {
    string name;
} LabelTokenData;

typedef struct LABEL_DISPLAY_TOKEN_DATA_T {
    string_view id;
    i64 end_c_idx;
} LabelDisplayTokenData;

typedef struct BIBLE_BLOCK_TOKEN_DATA_T {
    BiblePassages passages;
} BibleBlockTokenData;

typedef struct BIBLE_HOVER_TOKEN_DATA_T {
    BiblePassages passages;
    i64 end_c_idx;
} BibleHoverTokenData;

typedef BibleHoverTokenData BibleCiteTokenData;

typedef struct CITE_TOKEN_DATA_T {
    string_views key_sections;
    i64 end_c_idx;
} CiteTokenData;

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
        RegularTextTokenData reg_text;
        ItalicTextTokenData it_text;
        BoldTextTokenData bold_text;
        LabelTokenData label;
        LabelDisplayTokenData label_display;
        BibleBlockTokenData bible_block;
        BibleHoverTokenData bible_hover;
        BibleCiteTokenData bible_cite;
        CiteTokenData cite;
        BlockQuoteTokenData block_quote;
    } data;
} ArticleToken;

typedef struct ARTICLE_TOKENS_T {
    ARRAY_FIELDS(ArticleToken)
} ArticleTokens;

typedef struct LABELS_MAP_T {
    HASHMAP_FIELDS(const char*, string_view)
} LabelsMap;

typedef struct CITE_KEY_SEEN_MAP_T {
    HASHMAP_FIELDS(const char*, bool);
} CiteKeySeenMap;

typedef struct CITE_KEY_INFO_INDEX_MAP_T {
    HASHMAP_FIELDS(const char*, i64)
} CiteKeyInfoIndexMap;

typedef struct CITE_STRINGS_T {
    ARRAY_FIELDS(char*)
} MallocStrings;

typedef enum METABLOCK_KEY_E : i32 {
#ifndef X_METABLOCK_KEYS
#define X_METABLOCK_KEYS \
    X(NONE) \
    X(LABEL) \
    X(BIBLE) \
    X(CITE) \
    X(COUNT)
#endif
#ifndef X
#define X(key) \
    METABLOCK_KEY_##key,
#endif
    X_METABLOCK_KEYS
#undef X
} MetablockKey;

static const char *kMetablockKeyStrs[] = {
#ifndef X
#define X(key) \
    #key,
#endif
    X_METABLOCK_KEYS
#undef X
};

static const char *kMetablockStartDelimiter = "{{";
static const char *kMetablockEndDelimiter = "}}";
static const char *kMetablockLabelKey = "label";
static const char *kBlockQuotePrefix = "> ";

static int SortMallocStrings(const void *left, const void *right) {
    u64 left_count = strlen(left);
    u64 right_count = strlen(right);
    u64 min_count = left_count < right_count ? left_count : right_count;

    return strncmp(left, right, min_count);
}

static MetablockRange metablock_find_range(Arena *arena, const string_view *str_view) {
    MetablockRange range = {-1, -1};

    const i64 arena_start_offset = arena->offset;

    string str = str_make_view(arena, str_view);

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

    str_strip(&contents);

    string_views terms = str_split(arena, &contents, " ");

    if (terms.len < 2) {
        return false;
    }

    if (strncmp(terms.data[0].data, kMetablockLabelKey, strlen(kMetablockLabelKey)) != 0) {
        return false;
    }

    string_view display_str = HASHMAP_GET_VAL(in_labels_map, &terms.data[1].data);

    if (display_str.data) {
        return false;
    }

    out_label_tk->name = str_make_view(arena, &terms.data[1]);

    return true;
}

static i64 find_parent_open_tk_idx(const ArticleTokens *tks, i64 child_open_tk_idx) {
    i64 idx = -1;

    assert(child_open_tk_idx >= 0 && child_open_tk_idx < tks->len);

    i64 current_child_tk_idx = child_open_tk_idx;

    i64 unclosed_child_tk_count = 0;

    while (idx < 0 && current_child_tk_idx > 0) {
        if (unclosed_child_tk_count == 0) {
            ArticleToken *child_open_tk = ARRAY_ELEM(tks, &current_child_tk_idx);
            assert(child_open_tk->paren == TOKEN_PAREN_OPEN);
        }

        i64 prior_tk_idx = current_child_tk_idx - 1;
        ArticleToken *prior_tk = ARRAY_ELEM(tks, &prior_tk_idx);

        switch (prior_tk->paren) {
            case TOKEN_PAREN_CLOSE: {
                unclosed_child_tk_count++;
                break;
            }
            case TOKEN_PAREN_OPEN: {
                if (unclosed_child_tk_count > 0) {
                    unclosed_child_tk_count--;
                } else {
                    idx = prior_tk_idx;
                }
                break;
            }
            default:
                assert(0);
                break;
        }

        current_child_tk_idx--;
    }

    return idx;
}

static i64 find_closing_tk_idx(const ArticleTokens *tks, i64 open_tk_idx) {
    i64 idx = -1;

    assert(open_tk_idx >= 0 && open_tk_idx < tks->len);
    ArticleToken *open_tk = ARRAY_ELEM(tks, &open_tk_idx);
    assert(open_tk->paren == TOKEN_PAREN_OPEN);

    i64 current_tk_idx = open_tk_idx;

    i64 nested_tk_count = 0;

    while (idx < 0 && current_tk_idx < tks->len - 1) {
        i64 next_tk_idx = current_tk_idx + 1;

        ArticleToken *next_open_tk = ARRAY_ELEM(tks, &next_tk_idx);

        switch (next_open_tk->paren) {
            case TOKEN_PAREN_CLOSE: {
                if (nested_tk_count > 0) {
                    nested_tk_count--;
                } else {
                    idx = next_tk_idx;
                }
                break;
            }
            case TOKEN_PAREN_OPEN: {
                nested_tk_count++;
                break;
            }
            default:
                assert(0);
                break;
        }

        current_tk_idx++;
    }

    return idx;
}

typedef struct METABLOCK_DATA_T {
    MetablockRange range;
    MetablockKey key;
    string_views val_strs;
} MetablockData;

static MetablockData metablock_get_data(Arena *arena, const string_view *line_view) {
    MetablockData metablock_data = {
        METABLOCK_KEY_COUNT
    };

    metablock_data.range = metablock_find_range(arena, line_view);

    if (metablock_data.range.start_c_idx >= 0 && metablock_data.range.end_c_idx >= 0) {
        string_view metablock_view = {
            line_view->data + metablock_data.range.start_c_idx,
            metablock_data.range.end_c_idx - metablock_data.range.start_c_idx
        };

        str_advance(&metablock_view, (i64) strlen(kMetablockStartDelimiter));
        str_strip(&metablock_view);

        str_strip(&metablock_view);
        metablock_data.val_strs = str_split(arena, &metablock_view, " ");
        const string_view *key_str = &metablock_data.val_strs.data[0];

        for (i32 key_idx = 0; key_idx < METABLOCK_KEY_COUNT; key_idx++) {
            string current_key_str = str_make(arena, "%s", kMetablockKeyStrs[key_idx]);
            str_to_lower(&current_key_str);

            if (strncmp(key_str->data, current_key_str.data, current_key_str.len) == 0) {
                metablock_data.key = key_idx;
                break;
            }
        }
    }

    return metablock_data;
}

static string metablock_join_val_strs(Arena *arena, const string_views *val_strs, i64 start_idx) {
    assert(start_idx < val_strs->len);

    string ref_str = str_make(arena, "");

    for (
        i64 val_str_idx = start_idx;
        val_str_idx < val_strs->len;
        val_str_idx++
    ) {
        str_append(&ref_str, "%.*s", val_strs->data[val_str_idx].len, val_strs->data[val_str_idx].data);

        if (val_str_idx < val_strs->len - 1) {
            str_append(&ref_str, " ");
        }
    }

    return ref_str;
}

void body_to_html(
    Arena *arena,
    MetadataMap *metadata,
    bool bib_db_opened,
    const string_views *file_lines,
    i64 body_start_line_idx,
    string *out_html
) {
    if (body_start_line_idx < 0 || body_start_line_idx >= file_lines->len) {
        return;
    }

    ArticleTokens tks = {arena};
    ARRAY_MAKE(&tks);

    LabelsMap existing_labels = {HASHMAP_TYPE_STR_KEY};
    string_view default_label_val = {};
    HASHMAP_MAKE(&existing_labels, &default_label_val);

    BibleRefsSeenMap bible_refs_seen = {HASHMAP_TYPE_STR_KEY};
    bool bible_ref_unseen = false;
    HASHMAP_MAKE(&bible_refs_seen, &bible_ref_unseen);

    bool lsb_bible_quoted = false;

    i64 current_open_tk_idx = -1;

    for (i64 line_idx = body_start_line_idx; line_idx < file_lines->len; line_idx++) {
        const string_view *line = &file_lines->data[line_idx];

        string_view line_view = {line->data, line->len};

        if (current_open_tk_idx < 0) {
            str_strip(&line_view);

            if (line_view.len > 0) {
                switch (line_view.data[0]) {
                    case '#': {
                        // Heading
                        ArticleTokenType heading_tk_type = ARTICLE_TOKEN_TYPE_HEADING;
                        ArticleToken heading_open_tk = {
                            TOKEN_PAREN_OPEN,
                            heading_tk_type,
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

                                label_present = label_get_metablock(arena, &existing_labels, &metablock_view,
                                                                    &label_tk_data);
                                if (label_present) {
                                    text_start_idx = (i32) (label_range.end_c_idx + strlen(kMetablockEndDelimiter));
                                }
                            }
                        }

                        str_advance(&line_view, text_start_idx);
                        str_strip(&line_view);
                        heading_open_tk.data.heading.text = str_make_view(arena, &line_view);
                        ARRAY_PUSH(&tks, &heading_open_tk);

                        if (label_present) {
                            ArticleToken label_open_tk = {
                                TOKEN_PAREN_OPEN,
                                ARTICLE_TOKEN_TYPE_LABEL
                            };
                            label_open_tk.data.label = label_tk_data;

                            HASHMAP_PUT(&existing_labels, &label_tk_data.name.data, &line_view);

                            ARRAY_PUSH(&tks, &label_open_tk);

                            ArticleToken label_close_tk = {
                                TOKEN_PAREN_CLOSE,
                                ARTICLE_TOKEN_TYPE_LABEL
                            };

                            ARRAY_PUSH(&tks, &label_close_tk);
                        }

                        ArticleToken heading_close_tk = {
                            TOKEN_PAREN_CLOSE,
                            heading_tk_type,
                        };

                        ARRAY_PUSH(&tks, &heading_close_tk);

                        break;
                    }
                    case '>': {
                        ArticleToken open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_BLOCK_QUOTE,
                        };

                        BlockQuoteTokenData quote_tk_data = {};
                        quote_tk_data.text = str_make(arena, "");

                        open_tk.data.block_quote = quote_tk_data;

                        current_open_tk_idx = tks.len;

                        ARRAY_PUSH(&tks, &open_tk);

                        line_idx--;

                        break;
                    }
                    case '-': {
                        if (line->len <= 1 || line->data[1] != ' ') {
                            break;
                        }

                        ArticleToken ul_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_UNORDERED_LIST
                        };

                        ARRAY_PUSH(&tks, &ul_open_tk);

                        ArticleToken li_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_LIST_ITEM
                        };

                        ARRAY_PUSH(&tks, &li_open_tk);

                        current_open_tk_idx = tks.len;

                        ArticleToken text_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                        };

                        RegularTextTokenData text_data = {
                            .text = str_make(arena, ""),
                            .start_c_idx = 2,
                            .start_line_idx = line_idx,
                        };

                        text_open_tk.data.reg_text = text_data;

                        ARRAY_PUSH(&tks, &text_open_tk);

                        line_idx--;

                        break;
                    }
                    case '{': {
                        // Metablock
                        MetablockData metablock_data = metablock_get_data(arena, &line_view);

                        switch (metablock_data.key) {
                            case METABLOCK_KEY_BIBLE: {
                                if (metablock_data.val_strs.len >= 3) {
                                    const string_view *subkey_str = &metablock_data.val_strs.data[1];

                                    switch (bible_get_subkey(subkey_str)) {
                                        case BIBLE_SUBKEY_BLOCK: {
                                            string verse_refs_str = metablock_join_val_strs(
                                                arena,
                                                &metablock_data.val_strs,
                                                2
                                            );

                                            BibleBlockTokenData block_data = {
                                                .passages = bible_parse_ref(arena, &verse_refs_str),
                                            };

                                            lsb_bible_quoted = true;

                                            ArticleToken open_tk = {
                                                TOKEN_PAREN_OPEN,
                                                ARTICLE_TOKEN_TYPE_BIBLE_BLOCK
                                            };

                                            open_tk.data.bible_block = block_data;

                                            ARRAY_PUSH(&tks, &open_tk);

                                            ArticleToken close_tk = {
                                                TOKEN_PAREN_CLOSE,
                                                ARTICLE_TOKEN_TYPE_BIBLE_BLOCK
                                            };

                                            ARRAY_PUSH(&tks, &close_tk);

                                            break;
                                        }
                                        case BIBLE_SUBKEY_CITE:
                                        case BIBLE_SUBKEY_HOVER: {
                                            ArticleToken tk = {
                                                TOKEN_PAREN_OPEN,
                                                ARTICLE_TOKEN_TYPE_PARAGRAPH,
                                            };

                                            current_open_tk_idx = tks.len;

                                            ARRAY_PUSH(&tks, &tk);

                                            line_idx--;

                                            break;
                                        }
                                        default:
                                            break;
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                        }

                        break;
                    }
                    default: // Paragraph
                    {
                        ArticleToken p_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_PARAGRAPH
                        };

                        current_open_tk_idx = tks.len;

                        ARRAY_PUSH(&tks, &p_open_tk);

                        line_idx--;

                        break;
                    }
                }
            } else {
                current_open_tk_idx = -1;
            }
        } else {
            ArticleToken *current_open_tk = ARRAY_ELEM(&tks, &current_open_tk_idx);

            if (line->len > 0) {
                switch (current_open_tk->type) {
                    case ARTICLE_TOKEN_TYPE_PARAGRAPH: {
                        ArticleToken reg_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                        };

                        reg_open_tk.data.reg_text.start_c_idx = 0;
                        reg_open_tk.data.reg_text.start_line_idx = line_idx;
                        reg_open_tk.data.reg_text.text = str_make(arena, "");

                        current_open_tk_idx = tks.len;

                        ARRAY_PUSH(&tks, &reg_open_tk);

                        line_idx--;

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_REGULAR_TEXT: {
                        i64 start_char_idx = current_open_tk->data.reg_text.start_line_idx == line_idx
                                                 ? current_open_tk->data.reg_text.start_c_idx
                                                 : 0;

                        bool early_exit = false;

                        if (start_char_idx == 0) {
                            char start_c = line->data[0];
                            switch (start_c) {
                                case '-': {
                                    if (line->len <= 1 || line->data[1] != ' ') {
                                        break;
                                    }

                                    i64 parent_tk_idx = find_parent_open_tk_idx(&tks, current_open_tk_idx);
                                    if (parent_tk_idx < 0) {
                                        break;
                                    }
                                    ArticleToken *parent_tk = ARRAY_ELEM(&tks, &parent_tk_idx);

                                    if (parent_tk->type != ARTICLE_TOKEN_TYPE_LIST_ITEM) {
                                        break;
                                    }

                                    ArticleToken close_tk = {
                                        TOKEN_PAREN_CLOSE,
                                        ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                                    };

                                    ARRAY_PUSH(&tks, &close_tk);

                                    ArticleToken li_close_tk = {
                                        TOKEN_PAREN_CLOSE,
                                        ARTICLE_TOKEN_TYPE_LIST_ITEM
                                    };

                                    ARRAY_PUSH(&tks, &li_close_tk);

                                    ArticleToken li_open_tk = {
                                        TOKEN_PAREN_OPEN,
                                        ARTICLE_TOKEN_TYPE_LIST_ITEM
                                    };

                                    ARRAY_PUSH(&tks, &li_open_tk);

                                    ArticleToken text_open_tk = {
                                        TOKEN_PAREN_OPEN,
                                        ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                                    };

                                    RegularTextTokenData text_data = {
                                        .text = str_make(arena, ""),
                                        .start_c_idx = 2,
                                        .start_line_idx = line_idx,
                                    };

                                    text_open_tk.data.reg_text = text_data;

                                    current_open_tk_idx = tks.len;
                                    ARRAY_PUSH(&tks, &text_open_tk);

                                    line_idx--;

                                    early_exit = true;
                                    break;
                                }
                                default:
                                    break;
                            }
                        }

                        if (early_exit) {
                            break;
                        }

                        for (i64 c_idx = start_char_idx; c_idx < line->len; c_idx++) {
                            char c = line->data[c_idx];

                            bool new_tk = false;

                            ArticleToken open_tk = {
                                TOKEN_PAREN_OPEN,
                            };

                            switch (c) {
                                case '*': {
                                    bool is_italic = true;
                                    if (c_idx < line->len - 1 && line->data[c_idx + 1] == '*') {
                                        is_italic = false;
                                    }

                                    TextTokenData text_tk_data = {
                                        .start_line_idx = line_idx,
                                        .start_c_idx = is_italic ? c_idx + 1 : c_idx + 2,
                                        .text = str_make(arena, "")
                                    };

                                    if (is_italic) {
                                        open_tk.type = ARTICLE_TOKEN_TYPE_ITALIC_TEXT;
                                        open_tk.data.it_text = text_tk_data;
                                    } else {
                                        open_tk.type = ARTICLE_TOKEN_TYPE_BOLD_TEXT;
                                        open_tk.data.bold_text = text_tk_data;
                                    }

                                    new_tk = true;

                                    break;
                                }
                                case '{': {
                                    string_view metablock_view = {
                                        line->data + c_idx,
                                        line->len - c_idx,
                                    };

                                    MetablockData metablock_data = metablock_get_data(arena, &metablock_view);

                                    switch (metablock_data.key) {
                                        case METABLOCK_KEY_BIBLE: {
                                            if (metablock_data.val_strs.len >= 3) {
                                                const string_view *subkey_str = &metablock_data.val_strs.data[1];
                                                BibleSubkey subkey = bible_get_subkey(subkey_str);
                                                switch (subkey) {
                                                    case BIBLE_SUBKEY_HOVER:
                                                    case BIBLE_SUBKEY_CITE: {
                                                        string verse_ref_str = metablock_join_val_strs(
                                                            arena,
                                                            &metablock_data.val_strs,
                                                            2
                                                        );

                                                        BiblePassages passages = bible_parse_ref(
                                                            arena,
                                                            &verse_ref_str
                                                        );

                                                        lsb_bible_quoted = true;

                                                        i64 end_c_idx = c_idx
                                                                        + metablock_data.range.end_c_idx
                                                                        + (i64) strlen(kMetablockEndDelimiter);

                                                        if (subkey == BIBLE_SUBKEY_HOVER) {
                                                            open_tk.type = ARTICLE_TOKEN_TYPE_BIBLE_HOVER;
                                                            open_tk.data.bible_hover.passages = passages;
                                                            open_tk.data.bible_hover.end_c_idx = end_c_idx;
                                                        } else {
                                                            open_tk.type = ARTICLE_TOKEN_TYPE_BIBLE_CITE;
                                                            open_tk.data.bible_cite.passages = passages;
                                                            open_tk.data.bible_cite.end_c_idx = end_c_idx;
                                                        }

                                                        new_tk = true;

                                                        break;
                                                    }
                                                    default:
                                                        break;
                                                }
                                            }

                                            break;
                                        }
                                        case METABLOCK_KEY_CITE: {
                                            if (metablock_data.val_strs.len < 2) {
                                                break;
                                            }

                                            string cite_infos_str = metablock_join_val_strs(
                                                arena,
                                                &metablock_data.val_strs,
                                                1
                                            );

                                            string_views cite_infos = {arena};
                                            ARRAY_MAKE(&cite_infos);

                                            i64 cite_start_c_idx = 0;

                                            bool info_bracket_open = false;

                                            for (i64 cite_c_idx = 0; cite_c_idx < cite_infos_str.len; cite_c_idx++) {
                                                char cite_c = cite_infos_str.data[cite_c_idx];

                                                switch (cite_c) {
                                                    case ' ': {
                                                        if (!info_bracket_open) {
                                                            i64 end_cite_info_c_idx = cite_c_idx;

                                                            i64 prior_cite_c_idx = cite_c_idx - 1;
                                                            if (prior_cite_c_idx < 0) {
                                                                break;
                                                            }

                                                            char prior_cite_c = cite_infos_str.data[prior_cite_c_idx];
                                                            if (prior_cite_c_idx == ',') {
                                                                end_cite_info_c_idx = prior_cite_c_idx;
                                                            }

                                                            string_view cite_info = {
                                                                cite_infos_str.data + cite_start_c_idx,
                                                                end_cite_info_c_idx - cite_start_c_idx,
                                                            };

                                                            ARRAY_PUSH(&cite_infos, &cite_info);

                                                            cite_start_c_idx = cite_c_idx + 1;
                                                        }
                                                        break;
                                                    }
                                                    case '[': {
                                                        info_bracket_open = true;
                                                        break;
                                                    }
                                                    case ']': {
                                                        info_bracket_open = false;
                                                        break;
                                                    }
                                                    default:
                                                        break;
                                                }
                                            }

                                            string_view final_cite_info = {
                                                cite_infos_str.data + cite_start_c_idx,
                                                cite_infos_str.len - cite_start_c_idx,
                                            };

                                            ARRAY_PUSH(&cite_infos, &final_cite_info);

                                            CiteTokenData cite_data = {
                                                .key_sections = cite_infos,
                                                .end_c_idx = c_idx
                                                             + metablock_data.range.end_c_idx
                                                             + (i64) strlen(kMetablockEndDelimiter),
                                            };

                                            open_tk.type = ARTICLE_TOKEN_TYPE_CITE;
                                            open_tk.data.cite = cite_data;

                                            new_tk = true;

                                            break;
                                        }
                                        case METABLOCK_KEY_LABEL: {
                                            if (metablock_data.val_strs.len < 2) {
                                                break;
                                            }

                                            string_view label_id = metablock_data.val_strs.data[1];

                                            i64 end_c_idx = c_idx
                                                            + metablock_data.range.end_c_idx
                                                            + (i64) strlen(kMetablockEndDelimiter);

                                            open_tk.type = ARTICLE_TOKEN_TYPE_LABEL_DISPLAY;
                                            open_tk.data.label_display.id = label_id;
                                            open_tk.data.label_display.end_c_idx = end_c_idx;

                                            new_tk = true;

                                            break;
                                        }
                                        default:
                                            break;
                                    }

                                    break;
                                }
                                case '\t': {
                                    break;
                                }
                                default: {
                                    str_append(&current_open_tk->data.reg_text.text, "%c", c);
                                    break;
                                }
                            }

                            if (new_tk) {
                                ArticleToken reg_close_tk = {
                                    TOKEN_PAREN_CLOSE,
                                    ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                                };

                                ARRAY_PUSH(&tks, &reg_close_tk);

                                current_open_tk_idx = tks.len;
                                ARRAY_PUSH(&tks, &open_tk);
                                line_idx--;

                                break;
                            }

                            if (c_idx == line->len - 1) {
                                // Replace new line with a space
                                str_append(&current_open_tk->data.reg_text.text, " ");
                            }
                        }

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_ITALIC_TEXT: {
                        i64 start_c_idx = current_open_tk->data.it_text.start_line_idx == line_idx
                                              ? current_open_tk->data.it_text.start_c_idx
                                              : 0;

                        bool end_of_tk = false;
                        i64 c_idx;
                        for (c_idx = start_c_idx; c_idx < line->len; c_idx++) {
                            char c = line->data[c_idx];

                            if (c == '*') {
                                end_of_tk = true;
                                break;
                            }

                            str_append(&current_open_tk->data.it_text.text, "%c", c);

                            if (c_idx == line->len - 1) {
                                // Replace new line with a space
                                str_append(&current_open_tk->data.reg_text.text, " ");
                            }
                        }

                        if (end_of_tk) {
                            ArticleToken it_close_tk = {
                                TOKEN_PAREN_CLOSE,
                                ARTICLE_TOKEN_TYPE_ITALIC_TEXT
                            };

                            ARRAY_PUSH(&tks, &it_close_tk);

                            ArticleToken reg_open_tk = {
                                TOKEN_PAREN_OPEN,
                                ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                            };

                            reg_open_tk.data.reg_text.start_c_idx = c_idx + 1;
                            reg_open_tk.data.reg_text.start_line_idx = line_idx;
                            reg_open_tk.data.reg_text.text = str_make(arena, "");

                            current_open_tk_idx = tks.len;

                            ARRAY_PUSH(&tks, &reg_open_tk);

                            line_idx--;
                        }

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_BOLD_TEXT: {
                        i64 start_c_idx = current_open_tk->data.bold_text.start_line_idx == line_idx
                                              ? current_open_tk->data.bold_text.start_c_idx
                                              : 0;

                        bool end_of_tk = false;
                        i64 c_idx;
                        for (c_idx = start_c_idx; c_idx < line->len; c_idx++) {
                            if (c_idx < line->len - 1) {
                                char first_c = line->data[c_idx];
                                char second_c = line->data[c_idx + 1];

                                if (first_c == '*' && second_c == '*') {
                                    end_of_tk = true;
                                    break;
                                }
                            }

                            str_append(&current_open_tk->data.bold_text.text, "%c", line->data[c_idx]);

                            if (c_idx == line->len - 1) {
                                // Replace new line with a space
                                str_append(&current_open_tk->data.reg_text.text, " ");
                            }
                        }

                        if (end_of_tk) {
                            ArticleToken bold_close_tk = {
                                TOKEN_PAREN_CLOSE,
                                ARTICLE_TOKEN_TYPE_BOLD_TEXT
                            };

                            ARRAY_PUSH(&tks, &bold_close_tk);

                            ArticleToken reg_open_tk = {
                                TOKEN_PAREN_OPEN,
                                ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                            };

                            reg_open_tk.data.reg_text.start_c_idx = c_idx + 2;
                            reg_open_tk.data.reg_text.start_line_idx = line_idx;
                            reg_open_tk.data.reg_text.text = str_make(arena, "");

                            current_open_tk_idx = tks.len;

                            ARRAY_PUSH(&tks, &reg_open_tk);

                            line_idx--;
                        }

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_BIBLE_HOVER:
                    case ARTICLE_TOKEN_TYPE_BIBLE_CITE: {
                        ArticleToken close_tk = {
                            TOKEN_PAREN_CLOSE,
                            current_open_tk->type,
                        };

                        ARRAY_PUSH(&tks, &close_tk);

                        ArticleToken reg_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                        };

                        reg_open_tk.data.reg_text.start_line_idx = line_idx;
                        reg_open_tk.data.reg_text.start_c_idx = current_open_tk->type == ARTICLE_TOKEN_TYPE_BIBLE_HOVER
                                                                    ? current_open_tk->data.bible_hover.end_c_idx
                                                                    : current_open_tk->data.bible_cite.end_c_idx;
                        reg_open_tk.data.reg_text.text = str_make(arena, "");

                        current_open_tk_idx = tks.len;
                        ARRAY_PUSH(&tks, &reg_open_tk);

                        line_idx--;

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_CITE: {
                        ArticleToken close_tk = {
                            TOKEN_PAREN_CLOSE,
                            ARTICLE_TOKEN_TYPE_CITE,
                        };

                        ARRAY_PUSH(&tks, &close_tk);

                        ArticleToken reg_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                        };

                        reg_open_tk.data.reg_text.start_line_idx = line_idx;
                        reg_open_tk.data.reg_text.start_c_idx = current_open_tk->data.cite.end_c_idx;
                        reg_open_tk.data.reg_text.text = str_make(arena, "");

                        current_open_tk_idx = tks.len;
                        ARRAY_PUSH(&tks, &reg_open_tk);

                        line_idx--;

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_LABEL_DISPLAY: {
                        ArticleToken close_tk = {
                            TOKEN_PAREN_CLOSE,
                            ARTICLE_TOKEN_TYPE_LABEL_DISPLAY
                        };

                        ARRAY_PUSH(&tks, &close_tk);

                        ArticleToken reg_open_tk = {
                            TOKEN_PAREN_OPEN,
                            ARTICLE_TOKEN_TYPE_REGULAR_TEXT
                        };

                        reg_open_tk.data.reg_text.start_line_idx = line_idx;
                        reg_open_tk.data.reg_text.start_c_idx = current_open_tk->data.label_display.end_c_idx;
                        reg_open_tk.data.reg_text.text = str_make(arena, "");

                        current_open_tk_idx = tks.len;
                        ARRAY_PUSH(&tks, &reg_open_tk);

                        line_idx--;

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_BLOCK_QUOTE: {
                        if (line->len > 2
                            && strncmp(line->data, kBlockQuotePrefix, strlen(kBlockQuotePrefix)) == 0) {
                            for (i64 c_idx = 2; c_idx < line->len; c_idx++) {
                                str_append(&current_open_tk->data.block_quote.text, "%c", line->data[c_idx]);

                                if (c_idx == line->len - 1) {
                                    str_append(&current_open_tk->data.block_quote.text, " ");
                                }
                            }
                        } else {
                            ArticleToken close_tk = {
                                TOKEN_PAREN_CLOSE,
                                ARTICLE_TOKEN_TYPE_BLOCK_QUOTE
                            };

                            ARRAY_PUSH(&tks, &close_tk);
                            current_open_tk_idx = -1;

                            line_idx--;
                        }

                        break;
                    }
                    default: {
                        current_open_tk_idx = -1;
                        break;
                    }
                }
            } else {
                switch (current_open_tk->type) {
                    case ARTICLE_TOKEN_TYPE_BOLD_TEXT:
                    case ARTICLE_TOKEN_TYPE_ITALIC_TEXT:
                    case ARTICLE_TOKEN_TYPE_REGULAR_TEXT: {
                        ArticleToken close_tk = {
                            TOKEN_PAREN_CLOSE,
                            current_open_tk->type
                        };

                        ARRAY_PUSH(&tks, &close_tk);

                        i64 parent_open_tk_idx = find_parent_open_tk_idx(&tks, current_open_tk_idx);
                        assert(parent_open_tk_idx >= 0);

                        ArticleToken *parent_open_tk = ARRAY_ELEM(&tks, &parent_open_tk_idx);

                        switch (parent_open_tk->type) {
                            case ARTICLE_TOKEN_TYPE_LIST_ITEM: {
                                i64 list_parent_tk_idx = find_parent_open_tk_idx(&tks, parent_open_tk_idx);
                                assert(list_parent_tk_idx >= 0);

                                ArticleToken *list_parent_tk = ARRAY_ELEM(&tks, &list_parent_tk_idx);
                                assert(
                                    list_parent_tk->type == ARTICLE_TOKEN_TYPE_UNORDERED_LIST
                                    || list_parent_tk->type == ARTICLE_TOKEN_TYPE_ORDERED_LIST
                                );

                                ArticleToken li_close_tk = {
                                    TOKEN_PAREN_CLOSE,
                                    ARTICLE_TOKEN_TYPE_LIST_ITEM,
                                };

                                ARRAY_PUSH(&tks, &li_close_tk);

                                ArticleToken list_close_tk = {
                                    TOKEN_PAREN_CLOSE,
                                    list_parent_tk->type,
                                };

                                ARRAY_PUSH(&tks, &list_close_tk);

                                break;
                            }
                            case ARTICLE_TOKEN_TYPE_PARAGRAPH:
                            default: {
                                ArticleToken paragraph_close_tk = {
                                    TOKEN_PAREN_CLOSE,
                                    ARTICLE_TOKEN_TYPE_PARAGRAPH,
                                };

                                ARRAY_PUSH(&tks, &paragraph_close_tk);
                                break;
                            }
                        }

                        current_open_tk_idx = -1;

                        break;
                    }
                    case ARTICLE_TOKEN_TYPE_BLOCK_QUOTE: {
                        ArticleToken close_tk = {
                            TOKEN_PAREN_CLOSE,
                            ARTICLE_TOKEN_TYPE_BLOCK_QUOTE
                        };

                        ARRAY_PUSH(&tks, &close_tk);

                        current_open_tk_idx = -1;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    while (current_open_tk_idx >= 0) {
        ArticleToken *current_open_tk = ARRAY_ELEM(&tks, &current_open_tk_idx);
        assert(current_open_tk->paren == TOKEN_PAREN_OPEN);

        ArticleToken close_tk = {
            TOKEN_PAREN_CLOSE,
            current_open_tk->type
        };

        ARRAY_PUSH(&tks, &close_tk);
        current_open_tk_idx = find_parent_open_tk_idx(&tks, current_open_tk_idx);
    }

    CiteKeySeenMap cite_keys_seen = {HASHMAP_TYPE_STR_KEY};
    bool default_cite_keys_unseen = false;
    HASHMAP_MAKE(&cite_keys_seen, &default_cite_keys_unseen);

    CiteKeyInfoIndexMap unique_cite_entry_idxs = {HASHMAP_TYPE_STR_KEY};
    i64 default_cite_key_info_idx = -1;
    HASHMAP_MAKE(&unique_cite_entry_idxs, &default_cite_key_info_idx);

    MallocStrings cite_strs = {arena};
    ARRAY_MAKE(&cite_strs);

    i64 current_tk_idx = 0;

    i64 superscript_count = 0;

    while (current_tk_idx >= 0 && current_tk_idx < tks.len) {
        ArticleToken *current_tk = ARRAY_ELEM(&tks, &current_tk_idx);

        switch (current_tk->type) {
            case ARTICLE_TOKEN_TYPE_HEADING: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                i64 closing_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(closing_tk_idx >= 0);

                LabelTokenData *label_data = nullptr;
                if (closing_tk_idx - current_tk_idx > 1) {
                    i64 next_tk_idx = current_tk_idx + 1;
                    ArticleToken *next_tk = ARRAY_ELEM(&tks, &next_tk_idx);

                    if (next_tk->paren == TOKEN_PAREN_OPEN && next_tk->type == ARTICLE_TOKEN_TYPE_LABEL) {
                        label_data = &next_tk->data.label;

                        string_view label_display_name = {
                            current_tk->data.heading.text.data,
                            current_tk->data.heading.text.len
                        };
                        HASHMAP_PUT(&existing_labels, &label_data->name.data, &label_display_name);
                    }
                }

                i32 heading_level = current_tk->data.heading.level;
                str_append(out_html, "<h%d", heading_level);
                if (label_data) {
                    str_append(out_html, " id=\"%s\"", label_data->name.data);
                }
                str_append(out_html, ">");
                str_append(out_html, "%s", current_tk->data.heading.text.data);
                str_append(out_html, "</h%d>", heading_level);

                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);

                break;
            }
            case ARTICLE_TOKEN_TYPE_PARAGRAPH: {
                switch (current_tk->paren) {
                    case TOKEN_PAREN_OPEN: {
                        str_append(out_html, "<p>");
                        break;
                    }
                    case TOKEN_PAREN_CLOSE: {
                        str_append(out_html, "</p>");
                        break;
                    }
                    default:
                        assert(0);
                        break;
                }
                break;
            }
            case ARTICLE_TOKEN_TYPE_REGULAR_TEXT: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);
                str_append(out_html, "%s", current_tk->data.reg_text.text.data);
                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_ITALIC_TEXT: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);
                str_append(out_html, "<i>%s</i>", current_tk->data.it_text.text.data);
                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_BOLD_TEXT: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);
                str_append(out_html, "<b>%s</b>", current_tk->data.bold_text.text.data);
                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_BIBLE_BLOCK: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                str_append(out_html, "<div class=\"bible-block\">");
                const BiblePassages *passages = &current_tk->data.bible_block.passages;

                ARRAY_FOR(passage, passages) {
                    if (passage->book < BIBLE_BOOK_COUNT &&
                        passage->ch_v.chapter > 0 &&
                        passage->ch_v.start_verse > 0) {
                        i32 start_verse = passage->ch_v.start_verse;
                        i32 end_verse = passage->ch_v.end_verse;
                        if (end_verse < start_verse) {
                            end_verse = start_verse;
                        }

                        Arena tmp = arena_make(32 * (end_verse - start_verse + 1));

                        for (i32 current_verse = start_verse; current_verse <= end_verse; current_verse++) {
                            const char *verse_val = bible_get_verse(
                                passage->book,
                                passage->ch_v.chapter,
                                current_verse
                            );

                            if (verse_val) {
                                str_append(out_html, "%s", verse_val);
                            }
                        }

                        arena_free(&tmp);

                        str_append(out_html, "<p class=\"bible-block-verse-ref\">");
                        string ref_str = bible_passage_ref_to_str(arena, *passage);
                        str_append(out_html, "%s", ref_str.data);
                        str_append(out_html, "</p>");
                    }
                }

                str_append(out_html, "</div>");

                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_BIBLE_HOVER: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                str_append(out_html, "<span class=\"bible-hover\" >");

                const BiblePassages *passages = &current_tk->data.bible_hover.passages;
                Arena tmp = arena_make(512 + 64 * passages->len);
                DEFER(arena_free(&tmp)) {
                    for (i32 passage_idx = 0; passage_idx < passages->len; passage_idx++) {
                        string hover_html = bible_passage_to_hover_ref_html(
                            &tmp,
                            passages->data[passage_idx],
                            &bible_refs_seen
                        );
                        str_append(out_html, "%s", hover_html.data);

                        if (passage_idx < passages->len - 1) {
                            str_append(out_html, ", ");
                        }
                    }
                }

                str_append(out_html, "</span>");

                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_BIBLE_CITE: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                str_append(out_html, "<span class=\"bible-cite\">");

                str_append(out_html,
                           "<sup class=\"bible-cite-symbol\" "
                           "_=\""
                                "on mouseenter call OnBibleCiteMouseEnter(me)"
                                "on mouseleave call OnBibleCiteMouseLeave(me)"
                            "\">"
                               "\u271d"
                           "</sup>"
                );

                str_append(out_html, "<span class=\"bible-cite-refs hidden\">");

                const BiblePassages *passages = &current_tk->data.bible_cite.passages;

                Arena tmp = arena_make(512 + 64 * passages->len);
                DEFER(arena_free(&tmp)) {
                    for (i32 passage_idx = 0; passage_idx < passages->len; passage_idx++) {
                        string hover_html = bible_passage_to_hover_ref_html(
                            &tmp,
                            passages->data[passage_idx],
                            &bible_refs_seen
                        );

                        str_append(out_html, "<span class=\"bible-hover\" >");
                        str_append(out_html, "%s", hover_html.data);
                        str_append(out_html, "</span>");

                        if (passage_idx < passages->len - 1) {
                            str_append(out_html, ", ");
                        }
                    }
                }

                str_append(out_html, "</span>");

                str_append(out_html, "</span>");

                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_CITE: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                assert(bib_db_opened);


                ARRAY_FOR(cite_key_section_str, &current_tk->data.cite.key_sections) {
                    i64 open_bracket_idx = -1, close_bracket_idx = -1;

                    for (i64 c_idx = 0; c_idx < cite_key_section_str->len; c_idx++) {
                        switch (cite_key_section_str->data[c_idx]) {
                            case '[': {
                                open_bracket_idx = c_idx;
                                break;
                            }
                            case ']': {
                                close_bracket_idx = c_idx;
                                break;
                            }
                            default:
                                break;
                        }
                    }

                    if (close_bracket_idx < open_bracket_idx) {
                        continue;
                    }

                    string cite_key_str = str_make(arena, "");
                    string cite_section_str = str_make(arena, "");

                    if (open_bracket_idx > 0 && open_bracket_idx < close_bracket_idx - 1) {
                        string_view key_view = {
                            cite_key_section_str->data,
                            open_bracket_idx
                        };

                        string_view section_view = {
                            cite_key_section_str->data + open_bracket_idx + 1,
                            close_bracket_idx - open_bracket_idx - 1,
                        };

                        str_append(&cite_key_str, SV_FMT, SV_DATA(&key_view));
                        str_append(&cite_section_str, SV_FMT, SV_DATA(&section_view));
                    } else {
                        str_append(&cite_key_str, SV_FMT, SV_DATA(cite_key_section_str));
                    }

                    char *note = nullptr;

                    bool key_seen = HASHMAP_GET_VAL(&cite_keys_seen, &cite_key_str.data);
                    i64 unique_cite_idx = -1;

                    if (key_seen) {
                        unique_cite_idx = HASHMAP_GET_VAL(&unique_cite_entry_idxs, &cite_key_section_str->data);

                        if (unique_cite_idx < 0) {
                            note = bib_create_short_note_html(
                                cite_key_str.data,
                                CITE_STYLE_CHICAGO,
                                !str_empty(&cite_section_str) ? cite_section_str.data : nullptr
                            );

                            unique_cite_idx = cite_strs.len;
                            HASHMAP_PUT(&unique_cite_entry_idxs, &cite_key_section_str->data, &unique_cite_idx);

                            ARRAY_PUSH(&cite_strs, &note);
                        } else {
                            note = cite_strs.data[unique_cite_idx];
                        }
                    } else {
                        bool is_seen = true;
                        HASHMAP_PUT(&cite_keys_seen, &cite_key_str.data, &is_seen);

                        note = bib_create_note_html(
                            cite_key_str.data,
                            CITE_STYLE_CHICAGO,
                            !str_empty(&cite_section_str) ? cite_section_str.data : nullptr
                        );

                        unique_cite_idx = cite_strs.len;
                        HASHMAP_PUT(&unique_cite_entry_idxs, &cite_key_section_str->data, &unique_cite_idx);

                        ARRAY_PUSH(&cite_strs, &note);
                    }

                    assert(unique_cite_idx >= 0);
                    assert(note);

                    i64 display_cite_idx = unique_cite_idx + 1;

                    str_append(
                        out_html,
                        "<sup "
                        "id=\"cite-superscript-%d\" "
                        "class=\"cite-footnote-index\" "
                        "_=\"on click call OnCiteSuperscriptClick(me)\""
                        ">"
                        "<a href=\"#cite-footnote-%d\" >"
                        "%d"
                        "</a>"
                        "</sup>",
                        superscript_count,
                        display_cite_idx,
                        display_cite_idx
                    );

                    superscript_count++;
                }

                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_LABEL_DISPLAY: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                str_append(
                    out_html,
                    "<a class=\"label\" href=\"#" SV_FMT "\">",
                    SV_DATA(&current_tk->data.label_display.id)
                );

                string label_id_str = str_make_view(arena, &current_tk->data.label_display.id);

                string_view display_label = HASHMAP_GET_VAL(&existing_labels, &label_id_str.data);
                if (display_label.data) {
                    str_append(out_html, SV_FMT, SV_DATA(&display_label));
                }

                str_append(out_html, "</a>");

                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_BLOCK_QUOTE: {
                assert(current_tk->paren == TOKEN_PAREN_OPEN);

                str_append(
                    out_html,
                    "<blockquote>%s</blockquote>",
                    current_tk->data.block_quote.text.data
                );
                current_tk_idx = find_closing_tk_idx(&tks, current_tk_idx);
                assert(current_tk_idx >= 0);
                break;
            }
            case ARTICLE_TOKEN_TYPE_UNORDERED_LIST: {
                switch (current_tk->paren) {
                    case TOKEN_PAREN_OPEN: {
                        str_append(out_html, "<ul>");
                        break;
                    }
                    case TOKEN_PAREN_CLOSE: {
                        str_append(out_html, "</ul>");
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            case ARTICLE_TOKEN_TYPE_ORDERED_LIST: {
                switch (current_tk->paren) {
                    case TOKEN_PAREN_OPEN: {
                        str_append(out_html, "<ol>");
                        break;
                    }
                    case TOKEN_PAREN_CLOSE: {
                        str_append(out_html, "</ol>");
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            case ARTICLE_TOKEN_TYPE_LIST_ITEM: {
                switch (current_tk->paren) {
                    case TOKEN_PAREN_OPEN: {
                        str_append(out_html, "<li>");
                        break;
                    }
                    case TOKEN_PAREN_CLOSE: {
                        str_append(out_html, "</li>");
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }

        current_tk_idx++;
    }

    if (!ARRAY_EMPTY(&cite_strs)) {
        str_append(out_html, "<h1>Footnotes</h1>");
        str_append(out_html, "<ol class=\"cite-footnotes\">");

        for (i64 cite_idx = 0; cite_idx < cite_strs.len; cite_idx++) {
            str_append(out_html, "<li id=\"cite-footnote-%d\">", cite_idx + 1);
            str_append(out_html, "%s", cite_strs.data[cite_idx]);
            str_append(out_html, "</li>");
        }

        str_append(out_html, "</ol>");
    }

    i64 cite_keys_seen_len = HASHMAP_LEN(&cite_keys_seen);
    if (cite_keys_seen_len > 0) {
        str_append(out_html, "<h1>Bibliography</h1>");
        str_append(out_html, "<ul class=\"cite-bibliography\">");

        MallocStrings bib_strs = {arena, 0, cite_keys_seen_len};
        ARRAY_MAKE(&bib_strs);

        HASHMAP_FOR(seen_cite_pair, &cite_keys_seen) {
            const char *cite_key = seen_cite_pair->key;

            char *cite_bib_html = bib_create_bib_entry_html(
                cite_key,
                CITE_STYLE_CHICAGO
            );

            assert(cite_bib_html);

            ARRAY_PUSH(&bib_strs, &cite_bib_html);
        }

        ARRAY_SORT(&bib_strs, SortMallocStrings);

        ARRAY_FOR(bib_str, &bib_strs) {
            str_append(out_html, "<li class=\"cite-bibliography-entry\">");
            str_append(out_html, "%s", *bib_str);
            str_append(out_html, "</li>");
            free(*bib_str);
        }

        str_append(out_html, "</ul>");
    }

    if (lsb_bible_quoted) {
        str_append(out_html,
                   "<p class=\"copyright-footer\">"
                   "Scripture quotations taken from the (LSB®) Legacy Standard Bible®, "
                   "Copyright © 2021 by The Lockman Foundation. Used by permission. All rights reserved. "
                   "Managed in partnership with Three Sixteen Publishing Inc.&nbsp;"
                   "<a href=\"http://lsbible.org/\">LSBible.org</a>&nbsp;and&nbsp;"
                   "<a href=\"http://316publishing.com/\">316publishing.com</a>."
                   "</p>"
        );
    }

    i64 bible_refs_seen_len = HASHMAP_LEN(&bible_refs_seen);
    if (bible_refs_seen_len) {
        str_append(out_html, "<ul id=\"bible-refs-html\" class=\"hidden\">");
        HASHMAP_FOR(ref_key, &bible_refs_seen) {
            const char *verse_html = HASHMAP_GET_VAL(&g_lsb_verse_map, &ref_key->key);
            if (verse_html) {
                str_append(out_html, "<li id=\"%s\">", ref_key->key);
                str_append(out_html, "%s", verse_html);
                str_append(out_html, "</li>");
            }
        }
        str_append(out_html, "</ul>");
    }

    ARRAY_FOR(cite_str, &cite_strs) {
        free(*cite_str);
    }

    HASHMAP_FREE(&unique_cite_entry_idxs);

    HASHMAP_FREE(&cite_keys_seen);

    HASHMAP_FREE(&existing_labels);
}
