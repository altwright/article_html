//
// Created by wright on 3/8/26.
//

#include "bible.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "altcore/defer.h"
#include "bibtool_wrapper/libs/altcore/memory.h"

const char *kBibleSubkeyStrs[] = {
#ifndef X
#define X(subkey) \
#subkey,
#endif
    X_BIBLE_SUBKEYS
#undef X
};

static bool g_bible_initialised = false;

BibleVerseToHtmlMap g_lsb_verse_map = {HASHMAP_TYPE_STR_KEY, HASHMAP_DEL_FREQ_LOW};

const char *kBibleBookStrs[] = {
#ifndef X
#define X(book) \
    #book,
#endif
    X_BIBLE_BOOKS
#undef X
};

static const char *kBookNumStrs[] = {
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

        FILE *fp = fopen(lsb_csv_filepath, "rb");
        assert(fp);

        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        string csv_str = {&arena, fsize + 1};
        ARRAY_MAKE(&csv_str);
        memset(csv_str.data, 0, csv_str.len);

        u64 bytes_read = fread(csv_str.data, 1, fsize, fp);
        assert(bytes_read == fsize);

        string_view csv_view = {
            csv_str.data,
            csv_str.len,
        };

        string_views lines = str_split(&arena, &csv_view, "\n");

        for (i64 line_idx = 1; line_idx < lines.len; line_idx++) {
            const string_view *line = lines.data + line_idx;

            string_views comma_vals = str_split(&arena, line, ",");
            assert(comma_vals.len >= 3);

            const string_view *book_str = &comma_vals.data[0];
            const string_view *chapter_str = &comma_vals.data[1];
            const string_view *verse_str = &comma_vals.data[2];

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
                str_append(&bible_key_str, "%.*s_", book_str->len, book_str->data);
            }

            str_append(
                &bible_key_str,
                "%.*s_%.*s",
                chapter_str->len,
                chapter_str->data,
                verse_str->len,
                verse_str->data
            );
            str_to_upper(&bible_key_str);
            ARRAY_FOR(c, &bible_key_str) {
                if (*c == ' ') {
                    *c = '_';
                }
            }

            i64 text_start_idx = comma_vals.len > 3
                                     ? comma_vals.data[3].data - line->data
                                     : line->len;
            i64 text_len = line->len - text_start_idx;
            if (text_len > 0) {
                char *text_str = alt_calloc(text_len + 1, sizeof(char));
                assert(text_str);
                snprintf(
                    text_str,
                    text_len + 1,
                    "%.*s",
                    (i32) text_len,
                    line->data + (line->len - text_len)
                );

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

    string_view ref_view = {
        ref->data,
        ref->len,
    };
    string_views ref_vals = str_split(arena, &ref_view, " ");
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
            str_append(&book_str, "%s_" SV_FMT, book_num_str, SV_DATA(&ref_vals.data[ref_val_idx]));
        } else {
            str_append(&book_str, SV_FMT, SV_DATA(&ref_vals.data[ref_val_idx]));
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

        const string_view *ch_v_str = &ref_vals.data[ref_val_idx];

        string_views ch_v_str_parts = str_split(arena, ch_v_str, ",");

        for (i64 part_idx = 0; part_idx < ch_v_str_parts.len; part_idx++) {
            BibleChapterVerse ch_v = {};

            const string_view *ch_v_str_part = &ch_v_str_parts.data[part_idx];

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
                    BiblePassage passage = {
                        .book = book,
                    };

                    passage.ch_v.chapter = (i32) strtol(ch_v_str->data, nullptr, 10);

                    ARRAY_PUSH(&passages, &passage);

                    continue;
                }
            }

            string_view v_view = {};

            if (!ch_v.chapter) {
                string_view ch_view = {
                    ch_v_str_part->data,
                    colon_idx
                };

                string ch_str = str_make_view(arena, &ch_view);

                ch_v.chapter = (i32) strtol(ch_str.data, nullptr, 10);

                v_view.data = ch_v_str_part->data + colon_idx + 1;
                v_view.len = ch_v_str_part->len - colon_idx + 1;
            } else {
                v_view.data = ch_v_str_part->data;
                v_view.len = ch_v_str_part->len;
            }

            i64 dash_idx = -1;
            for (i64 c_idx = 0; c_idx < v_view.len; c_idx++) {
                char c = v_view.data[c_idx];
                if (c == '-') {
                    dash_idx = c_idx;
                    break;
                }
            }

            if (dash_idx < 0) {
                ch_v.start_verse = (i32) strtol(v_view.data, nullptr, 10);
            } else {
                string_views vs_views = str_split(arena, &v_view, "-");
                assert(vs_views.len == 2);
                const string_view *start_v = &vs_views.data[0];
                const string_view *end_v = &vs_views.data[1];
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

    const char *book_str = kBibleBookStrs[passage.book];
    const char *start_str = book_str;
    const char *num_str = nullptr;

    i32 num_idx = 0;
    for (num_idx = 0; num_idx < STATIC_ARRAY_LEN(kBookNumStrs); num_idx++) {
        char *substr = nullptr;
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

    str_strip(&book_name_view);

    book_name = str_make_view(arena, &book_name_view);

    for (i32 c_idx = 1; c_idx < book_name.len; c_idx++) {
        char *c = ARRAY_ELEM(&book_name, &c_idx);
        *c = (char) tolower(*c);
    }

    str_append(&ref_str, "%s %d", book_name.data, passage.ch_v.chapter);

    if (passage.ch_v.start_verse > 0) {
        str_append(&ref_str, ":%d", passage.ch_v.start_verse);
        if (passage.ch_v.start_verse < passage.ch_v.end_verse) {
            str_append(&ref_str, "-%d", passage.ch_v.end_verse);
        }
    }

    return ref_str;
}

BibleSubkey bible_get_subkey(const string_view *subkey_view) {
    BibleSubkey bible_subkey = BIBLE_SUBKEY_COUNT;

    Arena arena = arena_make((subkey_view->len + 1) + 32 * BIBLE_SUBKEY_COUNT);

    string subkey_str = str_make_view(&arena, subkey_view);

    for (i32 subkey_idx = 0; subkey_idx < BIBLE_SUBKEY_COUNT; subkey_idx++) {
        string current_subkey_str = str_make(
            &arena,
            "%s",
            kBibleSubkeyStrs[subkey_idx]
        );

        str_to_lower(&current_subkey_str);

        if (strncmp(
                subkey_str.data,
                current_subkey_str.data,
                current_subkey_str.len
            ) == 0
        ) {
            bible_subkey = subkey_idx;
            break;
        }
    }

    arena_free(&arena);

    return bible_subkey;
}

char *bible_get_verse(BibleBook book, i32 chapter, i32 verse) {
    Arena tmp = arena_make(64);

    string verse_key = str_make(
        &tmp,
        "%s_%d_%d",
        kBibleBookStrs[book],
        chapter,
        verse
    );

    arena_free(&tmp);

    char *verse_str = HASHMAP_GET_VAL(&g_lsb_verse_map, &verse_key.data);

    return verse_str;
}

string bible_verse_block_to_inline(Arena *arena, const char *verse) {
    string inline_html = str_make(arena, "%s", verse);

    const char* opening_block_elems[] = {"<div", "<h1", "<h2", "<h3"};

    for (i64 elem_idx = 0; elem_idx < STATIC_ARRAY_LEN(opening_block_elems); elem_idx++) {
        const char *opening_elem = opening_block_elems[elem_idx];
        i64 opening_elem_len = (i64) strlen(opening_elem);
        char *opening_elem_start = nullptr;
        while (opening_elem_start = strstr(inline_html.data, opening_elem), opening_elem_start) {
            i64 opening_elem_start_idx = opening_elem_start - inline_html.data;
            str_replace_at(
                &inline_html,
                opening_elem_start_idx,
                opening_elem_len,
                "<span"
            );
        }
    }

    const char* closing_block_elems[] = {"</div>", "</h1>", "</h2>", "</h3>"};

    for (i64 elem_idx = 0; elem_idx < STATIC_ARRAY_LEN(closing_block_elems); elem_idx++) {
        const char *closing_elem = closing_block_elems[elem_idx];
        i64 closing_elem_len = (i64) strlen(closing_elem);
        char *closing_elem_start = nullptr;
        while (closing_elem_start = strstr(inline_html.data, closing_elem), closing_elem_start) {
            i64 closing_elem_start_idx = closing_elem_start - inline_html.data;
            str_replace_at(
                &inline_html,
                closing_elem_start_idx,
                closing_elem_len,
                "</span>"
            );
        }
    }

    return inline_html;
}

string bible_passage_to_hover_ref_html(Arena *arena, BiblePassage passage) {
    string out_html = str_make(arena, "");

    if (passage.book < BIBLE_BOOK_COUNT
        && passage.ch_v.chapter > 0) {
        string ref_str = bible_passage_ref_to_str(arena, passage);

        str_append(
            &out_html,
            "<span class=\"bible-hover-ref\" _=\"on mouseenter call OnBibleRefHover()\">"
        );
        str_append(&out_html, "%s", ref_str.data);
        str_append(&out_html, "</span>");

        str_append(&out_html, "<span class=\"bible-hover-body hidden\">");

        if (passage.ch_v.start_verse > 0) {
            i32 start_verse = passage.ch_v.start_verse;
            i32 end_verse = passage.ch_v.end_verse;
            if (end_verse < start_verse) {
                end_verse = start_verse;
            }

            Arena tmp = arena_make(512 + 32 * (end_verse - start_verse + 1));
            DEFER(arena_free(&tmp)) {
                for (i32 current_verse = start_verse; current_verse <= end_verse; current_verse++) {
                    char *verse_val = bible_get_verse(
                        passage.book,
                        passage.ch_v.chapter,
                        current_verse
                    );

                    if (verse_val) {
                        string inline_verse_str = bible_verse_block_to_inline(&tmp, verse_val);
                        str_append(&out_html, "%s", inline_verse_str.data);
                    }
                }
            }
        } else {
            char *verse_val = bible_get_verse(passage.book, passage.ch_v.chapter, 1);
            if (verse_val) {
                Arena tmp = arena_make(512);
                DEFER(arena_free(&tmp)) {
                    string inline_verse_str = bible_verse_block_to_inline(&tmp, verse_val);
                    str_append(&out_html, "%s", inline_verse_str.data);
                }
            }
        }

        str_append(&out_html, "</span>");
    }

    return out_html;
}
