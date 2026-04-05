//
// Created by wright on 2/21/26.
//

#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include "library.h"

int main(int argc, char **argv) {
    article_init("./data/lsb.csv");

    ArticleData data = article_parse("data/baptism.xmd");
    if (data.body_html) {
        printf("%s\n", data.body_html);
    }

    article_free(&data);

    article_uninit();
}
