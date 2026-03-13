//
// Created by wright on 3/8/26.
//

#include "bible.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bibtool_wrapper/libs/altcore/memory.h"

static bool g_bible_initialised = false;

BibleVerseToHtmlMap g_lsb_verse_map = {HASHMAP_TYPE_STR_KEY};

const char *kBibleBookStrs[] = {
#ifndef X
#define X(book) \
    #book,
#endif
    X_BIBLE_BOOKS
#undef X
};

static const char* kBookNumStrs[] = {
    "FIRST",
    "SECOND",
    "THIRD",
};

static const char *book_num_to_str(i32 book_num) {
    const char *num_str = nullptr;

    if (book_num > 0 && book_num <= 3) {
        num_str = kBookNumStrs[book_num - 1];
    }

    return num_str;
}

void bible_init(const char *lsb_csv_filepath) {
    Arena arena = arena_make(200 * 1024 * 1024);

    if (!g_bible_initialised) {
        g_lsb_verse_map = (BibleVerseToHtmlMap){HASHMAP_TYPE_STR_KEY};

        char *default_val = nullptr;
        HASHMAP_MAKE(&g_lsb_verse_map, &default_val);

        FILE *fp = fopen("./data/lsb.csv", "rb");
        assert(fp);

        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        string csv_str = {&arena, fsize + 1};
        ARRAY_MAKE(&csv_str);
        memset(csv_str.data, 0, csv_str.len);

        u64 bytes_read = fread(csv_str.data, 1, fsize, fp);
        assert(bytes_read == fsize);

        strings lines = str_split(&arena, &csv_str, "\n");

        for (i64 line_idx = 1; line_idx < lines.len; line_idx++) {
            const string *line = lines.data + line_idx;

            i64s comma_idxs = str_split_idxs(&arena, line, ",");
            assert(comma_idxs.len >= 3);

            string_view b_ch_v_view = {
                .data = line->data,
                .len = comma_idxs.data[2],
            };

            string b_ch_v_str = str_view_make(&arena, &b_ch_v_view);

            strings b_ch_v_str_parts = str_split(&arena, &b_ch_v_str, ",");
            assert(b_ch_v_str_parts.len == 3);

            const string *book_str = &b_ch_v_str_parts.data[0];
            const string *chapter_str = &b_ch_v_str_parts.data[1];
            const string *verse_str = &b_ch_v_str_parts.data[2];

            string bible_key_str = str_make(&arena, "");

            bool is_numbered_book = false;

            if (isdigit(book_str->data[0])) {
                char *title_start = nullptr;
                i64 book_num = strtol(book_str->data, &title_start, 10);
                const char *num_str = book_num_to_str((i32) book_num);

                if (num_str && title_start) {
                    title_start++; // Skip the space
                    is_numbered_book = true;

                    string_view title_view = {
                        title_start,
                        (book_str->data + book_str->len) - title_start,
                    };
                    str_append(&bible_key_str, "%s_%.*s_", num_str, title_view.len, title_view.data);
                }
            }

            if (!is_numbered_book) {
                str_append(&bible_key_str, "%s_", book_str->data);
            }

            str_append(&bible_key_str, "%s_%s", chapter_str->data, verse_str->data);
            str_to_upper(&bible_key_str);
            ARRAY_FOR(c, &bible_key_str) {
                if (*c == ' ') {
                    *c = '_';
                }
            }

            string_view text_view = {
                line->data + comma_idxs.data[2] + 1,
            };
            text_view.len = (line->data + line->len) - text_view.data;
            if (text_view.len > 0) {
                char* text_str = alt_calloc(text_view.len + 1, sizeof(char));
                assert(text_str);
                snprintf(text_str, text_view.len, "%s", text_view.data);

                HASHMAP_PUT(&g_lsb_verse_map, &bible_key_str.data, &text_str);
            }
        }

        int err = fclose(fp);
        assert(!err);

        g_bible_initialised = true;
    }

    arena_free(&arena);
}

void bible_uninit() {
    if (g_bible_initialised) {
        HASHMAP_FOR(key_val, &g_lsb_verse_map) {
            alt_free(key_val->value);
        }

        HASHMAP_FREE(&g_lsb_verse_map);

        g_bible_initialised = false;
    }
}

BiblePassages bible_parse_ref(Arena *arena, const string *ref) {
    BiblePassages passages = {arena};
    ARRAY_MAKE(&passages);

    strings ref_vals = str_split(arena, ref, " ");
    i32 ref_val_idx = 0;

    while (ref_val_idx < ref_vals.len) {
        const char *book_num_str = nullptr;

        char first_c = ref_vals.data[ref_val_idx].data[0];
        if (isdigit(first_c)) {
            i64 book_num = strtol(ref_vals.data[ref_val_idx].data, nullptr, 10);
            book_num_str = book_num_to_str((i32) book_num);
            ref_val_idx++;
        }

        if (ref_val_idx >= ref_vals.len) {
            continue;
        }

        string book_str = str_make(arena, "");
        if (book_num_str) {
            str_append(&book_str, "%s_%s", book_num_str, ref_vals.data[ref_val_idx].data);
        } else {
            str_append(&book_str, "%s", ref_vals.data[ref_val_idx].data);
        }

        str_to_upper(&book_str);

        BibleBook book = BIBLE_BOOK_COUNT;

        char *substr_start = nullptr;
#ifndef X
#define X(bible_enum) \
        if (substr_start = strstr(#bible_enum, book_str.data), substr_start) \
        { \
            book = BIBLE_BOOK_##bible_enum; \
        } \
        else
#endif
        X_BIBLE_BOOKS;
#undef X

        if (book == BIBLE_BOOK_COUNT) {
            ref_val_idx = (i32) ref_vals.len;
            continue;
        }

        ref_val_idx++;

        if (ref_val_idx >= ref_vals.len) {
            continue;
        }

        const string *ch_v_str = &ref_vals.data[ref_val_idx];

        strings ch_v_str_parts = str_split(arena, ch_v_str, ",");

        for (i64 part_idx = 0; part_idx < ch_v_str_parts.len; part_idx++) {
            BibleChapterVerse ch_v = {};

            const string *ch_v_str_part = &ch_v_str_parts.data[part_idx];

            i64 colon_idx = -1;
            for (i64 c_idx = 0; c_idx < ch_v_str_part->len; c_idx++) {
                char c = ch_v_str_part->data[c_idx];
                if (c == ':') {
                    colon_idx = c_idx;
                    break;
                }
            }

            if (colon_idx == -1) {
                if (passages.len > 0) {
                    BiblePassage prior_passage = passages.data[passages.len - 1];
                    ch_v.chapter = prior_passage.ch_v.chapter;
                } else {
                    continue;
                }
            }

            string_view v_view = {};

            if (!ch_v.chapter) {
                string_view ch_view = {
                    ch_v_str_part->data,
                    colon_idx
                };

                string ch_str = str_view_make(arena, &ch_view);

                ch_v.chapter = (i32) strtol(ch_str.data, nullptr, 10);

                v_view.data = ch_v_str_part->data + colon_idx + 1;
                v_view.len = ch_v_str_part->len - colon_idx + 1;
            } else {
                v_view.data = ch_v_str_part->data;
                v_view.len = ch_v_str_part->len;
            }

            string v_str = str_view_make(arena, &v_view);

            i64 dash_idx = -1;
            for (i64 c_idx = 0; c_idx < v_str.len; c_idx++) {
                char c = v_str.data[c_idx];
                if (c == '-') {
                    dash_idx = c_idx;
                    break;
                }
            }

            if (dash_idx < 0) {
                ch_v.start_verse = (i32) strtol(v_str.data, nullptr, 10);
            } else {
                strings vs_str = str_split(arena, &v_str, "-");
                assert(vs_str.len == 2);
                const string *start_v = &vs_str.data[0];
                const string *end_v = &vs_str.data[1];
                ch_v.start_verse = (i32) strtol(start_v->data, nullptr, 10);
                ch_v.end_verse = (i32) strtol(end_v->data, nullptr, 10);
            }

            BiblePassage passage = {
                book,
                ch_v,
            };

            ARRAY_PUSH(&passages, &passage);
        }

        ref_val_idx++;
    }

    return passages;
}

string bible_passage_ref_to_str(Arena *arena, BiblePassage passage) {
    string ref_str = str_make(arena, "");

    if (passage.book >= BIBLE_BOOK_COUNT) {
        return ref_str;
    }

    const char* book_str = kBibleBookStrs[passage.book];
    const char* start_str = book_str;
    const char* num_str = nullptr;

    i32 num_idx = 0;
    for (num_idx = 0; num_idx < STATIC_ARRAY_LEN(kBookNumStrs); num_idx++) {
        char* substr = nullptr;
        if (substr = strstr(start_str, kBookNumStrs[num_idx]),
            substr && (substr == start_str)) {
            start_str += strlen(kBookNumStrs[num_idx]);
            break;
        }
    }

    if (num_idx < STATIC_ARRAY_LEN(kBookNumStrs)) {
        str_append(&ref_str, "%d ", num_idx + 1);
    }

    string book_name = str_make(arena, "%s", start_str);
    ARRAY_FOR(c, &book_name) {
        if (*c == '_') {
            *c = ' ';
        }
    }

    string_view book_name_view = {
        book_name.data,
        book_name.len,
    };

    str_view_strip(&book_name_view);

    book_name = str_view_make(arena, &book_name_view);

    for (i32 c_idx = 1; c_idx < book_name.len; c_idx++) {
        char *c = ARRAY_ELEM(&book_name, &c_idx);
        *c = (char)tolower(*c);
    }

    str_append(&ref_str, "%s %d:%d", book_name.data, passage.ch_v.chapter, passage.ch_v.start_verse);
    if (passage.ch_v.start_verse < passage.ch_v.end_verse) {
        str_append(&ref_str, "-%d", passage.ch_v.end_verse);
    }

    return ref_str;
}
