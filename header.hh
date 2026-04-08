/*
    ./header.hh
    Q@hackers.pk
 */
#ifndef CSV_PARSER_LIB_HEADER_HH
#define CSV_PARSER_LIB_HEADER_HH

#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>

/*
    Token Delimiter
    ---------------
    The token delimiter is the character that separates the tokens in the CSV file.
    By default, the token delimiter is a comma (',').

    What ever the delimiter is, just make sure t is not part of the 1. token 2. it is not included in the ALL_PUNCTUATION array.
 */
#ifndef CSV_PARSER_TOKEN_DELIMITER
#define CSV_PARSER_TOKEN_DELIMITER  ','
#endif

/*
    UTF‑8 decoding process, which follows a most‑significant‑first (big‑endian) order for the logical code point value
    That is most significant bits are stored at lower memory addresses.
 */
/*
    1-byte ASCII, 7-bit non-extended ASCII [ 01111111 ]
    2-byte, 11-bit [ 10xxxxxx 110xxxxx ]
    3-byte, 16-bit [ 10xxxxxx 10xxxxxx 1110xxxx ]
    4-byte, 21-bit [ 10xxxxxx 10xxxxxx 10xxxxxx 11110xxx ]
 */
/*
#define UTF_8_CODE_POINT_1_BYTE_MASK 0x00
#define UTF_8_CODE_POINT_2_BYTE_MASK 0x1F
#define UTF_8_CODE_POINT_3_BYTE_MASK 0x0F
#define UTF_8_CODE_POINT_4_BYTE_MASK 0x07

#define UTF_8_1_BYTE_MASK 0x80
#define UTF_8_2_BYTE_MASK 0xE0
#define UTF_8_3_BYTE_MASK 0xF0
#define UTF_8_4_BYTE_MASK 0xF8
 
#define UTF_8_MASKED_1_BYTE_VALUE 0x00
#define UTF_8_MASKED_2_BYTE_VALUE 0xC0
#define UTF_8_MASKED_3_BYTE_VALUE 0xE0
#define UTF_8_MASKED_4_BYTE_VALUE 0xF0

#define UTF_8_SUBSEQUENT_BYTE_MASK 0xC0
#define UTF_8_SUBSEQUENT_BYTE_MASKED_VALUE 0x80

#define UTF_8_SUBSIQUENT_BYTE_MASK 0x3F
#define UTF_8_SUBSIQUENT_BYTE_LEFT_SHIFT 6
*/

#include "./lib/PunctuationSymbols.hh"
#include "./lib/Parser.hh"

#endif // CSV_PARSER_LIB_HEADER_HH