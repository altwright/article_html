#ifndef ARTICLE_HTML_LIBRARY_H
#define ARTICLE_HTML_LIBRARY_H

void article_init();

void article_uninit();

char *article_to_html(const char *filepath);

#endif // ARTICLE_HTML_LIBRARY_H
