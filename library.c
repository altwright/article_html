#include "library.h"

#include <altcore/types.h>
#include <altcore/malloc.h>
#include <altcore/arenas.h>
#include <altcore/strings.h>


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

    string html = str_make(&tmp, "");

    char *html_malloc = nullptr;

    return html_malloc;
}
