//
// Created by wright on 2/21/26.
//

#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include "library.h"

int main(int argc, char ** argv) {
    for (int i = 0; i < 10; i++) {
        article_init();

        char * html = article_to_html("data/draft_1.xmd");
        if (html) {
            printf("%s\n", html);
            free(html);
        }

        article_uninit();
    }
}