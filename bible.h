//
// Created by wright on 3/8/26.
//

#ifndef ARTICLE_HTML_BIBLE_H
#define ARTICLE_HTML_BIBLE_H

#include <altcore/types.h>
#include <altcore/strings.h>
#include <altcore/arenas.h>

typedef enum BIBLE_BOOK_E : i32 {
#ifndef X_BIBLE_BOOKS
#define X_BIBLE_BOOKS \
    X(GENESIS) \
    X(EXODUS) \
    X(LEVITICUS) \
    X(NUMBERS) \
    X(DEUTERONOMY) \
    X(JOSHUA) \
    X(JUDGES) \
    X(RUTH) \
    X(FIRST_SAMUEL) \
    X(SECOND_SAMUEL) \
    X(FIRST_KINGS) \
    X(SECOND_KINGS) \
    X(FIRST_CHRONICLES) \
    X(SECOND_CHRONICLES) \
    X(EZRA) \
    X(NEHEMIAH) \
    X(ESTHER) \
    X(JOB) \
    X(PSALMS) \
    X(PROVERBS) \
    X(ECCLESIASTES) \
    X(SONG_OF_SOLOMON) \
    X(ISAIAH) \
    X(JEREMIAH) \
    X(LAMENTATIONS) \
    X(EZEKIEL) \
    X(DANIEL) \
    X(HOSEA) \
    X(JOEL) \
    X(AMOS) \
    X(OBADIAH) \
    X(JONAH) \
    X(MICAH) \
    X(NAHUM) \
    X(HABAKKUK) \
    X(ZEPHANIAH) \
    X(HAGGAI) \
    X(ZECHARIAH) \
    X(MALACHI) \
    X(MATTHEW) \
    X(MARK) \
    X(LUKE) \
    X(JOHN) \
    X(ACTS) \
    X(ROMANS) \
    X(FIRST_CORINTHIANS) \
    X(SECOND_CORINTHIANS) \
    X(GALATIANS) \
    X(EPHESIANS) \
    X(PHILIPPIANS) \
    X(COLOSSIANS) \
    X(FIRST_THESSALONIANS) \
    X(SECOND_THESSALONIANS) \
    X(FIRST_TIMOTHY) \
    X(SECOND_TIMOTHY) \
    X(TITUS) \
    X(PHILEMON) \
    X(HEBREWS) \
    X(JAMES) \
    X(FIRST_PETER) \
    X(SECOND_PETER) \
    X(FIRST_JOHN) \
    X(SECOND_JOHN) \
    X(THIRD_JOHN) \
    X(JUDE) \
    X(REVELATION) \
    X(COUNT)
#endif
#ifndef X
#define X(book) \
    BIBLE_BOOK_##book,
#endif
    X_BIBLE_BOOKS
#undef X
} BibleBook;

typedef struct BIBLE_CHAPTER_VERSE_T {
    i32 chapter;
    i32 start_verse;
    i32 end_verse;
} BibleChapterVerse;

typedef struct BIBLE_PASSAGE_T {
    BibleBook book;
    BibleChapterVerse ch_v;
} BiblePassage;

typedef struct BIBLE_PASSAGES_T {
    ARRAY_FIELDS(BiblePassage);
} BiblePassages;

BiblePassages bible_parse_ref(Arena* arena, const string* ref);

#endif //ARTICLE_HTML_BIBLE_H