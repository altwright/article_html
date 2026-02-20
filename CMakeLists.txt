cmake_minimum_required(VERSION 4.1)
project(article_html C)

set(CMAKE_C_STANDARD 23)

add_library(article_html STATIC library.c)
