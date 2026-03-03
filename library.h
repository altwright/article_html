#ifndef ARTICLE_HTML_LIBRARY_H
#define ARTICLE_HTML_LIBRARY_H

typedef struct ARTICLE_DATA_T {
    char* title;
    char* subtitle;
    char* author;
    char* date_created;
    char* date_modified;
    char* body_html;
} ArticleData;

void article_init();

void article_uninit();

ArticleData article_parse(const char *filepath);

void article_free(ArticleData *data);

#endif // ARTICLE_HTML_LIBRARY_H
