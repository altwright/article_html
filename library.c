#include "library.h"

#include <assert.h>
#include <stdio.h>
#include <altcore/types.h>
#include <altcore/memory.h>
#include <altcore/arenas.h>
#include <altcore/strings.h>
#include <altcore/hashmap.h>

#include "body.h"
#include "metadata.h"
#include "altcore/defer.h"

static bool g_initialized = false;
static i64 kMallocInitialCapacity = 1024LL * 1024LL;

void article_init() {
    if (!g_initialized) {
        alt_init(kMallocInitialCapacity);

        g_initialized = true;
    }
}

void article_uninit() {
    if (g_initialized) {
        alt_uninit();

        g_initialized = false;
    }
}

ArticleData article_parse(const char *filepath) {
    ArticleData data = {};
    if (!filepath) {
        return data;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        return data;
    }

    Arena tmp = arena_make(kMallocInitialCapacity / 2);
    string file_buffer = {&tmp};

    int err = 0;
    DEFER(err = fclose(fp), assert(!err), fp = nullptr) {
        err = fseek(fp, 0, SEEK_END);
        assert(!err);

        long file_size = ftell(fp);
        rewind(fp);

        file_buffer.len = file_size + 1;
        ARRAY_MAKE(&file_buffer);

        size_t read_size = fread(file_buffer.data, sizeof(char), file_size, fp);
        assert(read_size == file_size);

        file_buffer.data[file_size] = '\0';
    }

    strings file_lines = str_split(&tmp, &file_buffer, "\n");

    MetadataMap metadata_map = {HASHMAP_TYPE_STR_KEY};
    string default_str = {};
    HASHMAP_MAKE(&metadata_map, &default_str);

    i64 start_body_line_idx = metadata_get(&tmp, &file_lines, &metadata_map);
    if (start_body_line_idx >= 0) {
        string body_html = str_make(&tmp, "");

        body_to_html(&tmp, &metadata_map, &file_lines, start_body_line_idx, &body_html);

        data.body_html = calloc(body_html.len + 1, sizeof(char));
        assert(data.body_html);
        memcpy(data.body_html, body_html.data, body_html.len);
    }

    HASHMAP_FREE(&metadata_map);

    arena_free(&tmp);

    return data;
}

void article_free(ArticleData *data) {
    if (data) {
        if (data->title) {
            free(data->title);
            data->title = nullptr;
        }
        if (data->subtitle) {
            free(data->subtitle);
            data->subtitle = nullptr;
        }
        if (data->author) {
            free(data->author);
            data->author = nullptr;
        }
        if (data->date_created) {
            free(data->date_created);
            data->date_created = nullptr;
        }
        if (data->date_modified) {
            free(data->date_modified);
            data->date_modified = nullptr;
        }
        if (data->body_html) {
            free(data->body_html);
            data->body_html = nullptr;
        }
    }
}
