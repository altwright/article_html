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

char *article_to_html(const char *filepath) {
    if (!filepath) {
        return nullptr;
    }

    char *body_html_buf = nullptr;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        return nullptr;
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

        body_html_buf = calloc(body_html.len + 1, sizeof(char));
        assert(body_html_buf);
        memcpy(body_html_buf, body_html.data, body_html.len);
    }

    HASHMAP_FREE(&metadata_map);

    arena_free(&tmp);

    return body_html_buf;
}
