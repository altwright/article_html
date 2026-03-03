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

        ArticleData data = article_parse("data/draft_1.xmd");
        if (data.body_html) {
            printf("%s\n", data.body_html);
        }

        article_free(&data);

        article_uninit();
    }
}