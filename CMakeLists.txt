cmake_minimum_required(VERSION 4.1)
project(article_html C)

set(CMAKE_C_STANDARD 23)

add_library(article_html STATIC library.c
        metadata.c
        metadata.h
        test.c
        test.h)

add_executable(article_html_test
        test.c
)

add_subdirectory(libs/altcore)
add_subdirectory(libs/bibtool_wrapper)

target_include_directories(article_html PRIVATE
        libs/
)

target_link_libraries(article_html PRIVATE
        altcore
        bibtool_wrapper
)

target_include_directories(article_html_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(article_html_test PRIVATE
        article_html
)
