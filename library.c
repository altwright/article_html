#include "library.h"

#include <assert.h>
#include <stdio.h>
#include <altcore/types.h>
#include <altcore/memory.h>
#include <altcore/arenas.h>
#include <altcore/strings.h>
#include <altcore/hashmap.h>

#include "metadata.h"
#include "altcore/defer.h"

static Arena g_arena = {};
static bool g_initialized = false;

void article_init() {
    if (!g_initialized) {
        i64 malloc_cap = 1024LL * 1024LL;
        alt_init(malloc_cap);

        g_arena = arena_make(malloc_cap / 2);

        g_initialized = true;
    }
}

void article_uninit() {
    if (g_initialized) {
        arena_free(&g_arena);
        alt_uninit();

        g_initialized = false;
    }
}

char *article_to_html(const char *filepath) {
    if (!filepath) {
        return nullptr;
    }

    Arena tmp = g_arena;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        return nullptr;
    }

    char *html_malloc = nullptr;

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

    MetadataMap metadata_map = {HASHMAP_TYPE_STR_KEY};
    string default_str = {};
    HASHMAP_MAKE(&metadata_map, &default_str);

    DEFER(HASHMAP_FREE(&metadata_map)) {
        bool metadata_found = metadata_get(&tmp, &file_buffer, &metadata_map);
    }

    string html = str_make(&tmp, "");

    return html_malloc;
}
