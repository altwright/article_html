//
// Created by wright on 3/8/26.
//

#include "bible.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *kBibleBookStrs[] = {
#ifndef X
#define X(book) \
    #book,
#endif
    X_BIBLE_BOOKS
#undef X
};

static const char *book_num_to_str(i32 book_num) {
    const char *num_str = nullptr;

    switch (book_num) {
        case 1: {
            num_str = "FIRST";
            break;
        }
        case 2: {
            num_str = "SECOND";
            break;
        }
        case 3: {
            num_str = "THIRD";
            break;
        }
        default:
            break;
    }

    return num_str;
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
