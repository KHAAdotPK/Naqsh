/* 
    lib/src/PunctuationSymbols.hh 
    Q@hackers.pk
 */
#ifndef CSV_PARSER_LIB_PUNCTUATION_SYMBOLS_HH
#define CSV_PARSER_LIB_PUNCTUATION_SYMBOLS_HH

namespace UrduPunctuation
{

// Unicode code points for punctuation marks found in the Urdu text
constexpr char32_t EXCLAMATION_MARK      = U'\u0021'; // ! (33 decimal)
constexpr char32_t QUOTATION_MARK        = U'\u0022'; // " (34 decimal)
constexpr char32_t LEFT_PARENTHESIS      = U'\u0028'; // ( (40 decimal)
constexpr char32_t RIGHT_PARENTHESIS     = U'\u0029'; // ) (41 decimal)
constexpr char32_t PLUS_SIGN             = U'\u002B'; // + (43 decimal) // Can be replaced by a white space but between umbers it has a meaning and need 
constexpr char32_t FULL_STOP_LATIN       = U'\u002E'; // . (46 decimal)
constexpr char32_t SOLIDUS               = U'\u002F'; // / (47 decimal)
constexpr char32_t COLON                 = U'\u003A'; // : (58 decimal) // Colon is used in time format as in 10:45 (since COLON IS PART OF ALL_PUNCTUATION array we need to expicitly try to make it part of the cleaned text), any where else it be removed from the text
constexpr char32_t ARABIC_COMMA          = U'\u060C'; // ، (1548 decimal)
constexpr char32_t ARABIC_SEMICOLON      = U'\u061B'; // ؛ (1559 decimal)
constexpr char32_t ARABIC_QUESTION_MARK  = U'\u061F'; // ؟ (1567 decimal)
constexpr char32_t ARABIC_FULL_STOP      = U'\u06D4'; // ۔ (1748 decimal)
/*
    Zero‑Width Non‑Joiner (ZWNJ)
    Zero‑Width Joiner (ZWJ)

    For Urdu, a common rule is: tokens are separated by spaces, punctuation, or ZWNJ.
    ZWNJ is a candidate for removel:
    1. At the end of the token it will just be removed from the stream
    2. Between two tokens it will replaced by a space.

    For Urdu ZWJ is not a separator; it indicates that two characters should be joined into one glyph,
    but logically they are still part of the same token.
    The ZWJ can be used between two characters to force a joined form; still one token.
    For this implementation we are not considering ZWJ as a candidate for removal, we will keep it there as part of the token.
 */
// Zero‑Width Non‑Joiner
constexpr char32_t ZERO_WIDTH_NON_JOINER = U'\u200C'; // Zero Width Non-Joiner (ZWNJ) invisible formatting character
// Zero‑Width Joiner
constexpr char32_t ZERO_WIDTH_JOINER     = U'\u200D'; // Zero Width Joiner (ZWJ) invisible formatting character

/*
 * Group-wise macros for optional punctuation/symbols in Urdu text normalization.
 * All macros expand to a Unicode character literal (char32_t).
 */

// ==================== 1. Hyphens and Dashes ====================
constexpr char32_t HYPHEN_MINUS = U'\u002D'; // -
constexpr char32_t EN_DASH      = U'\u2013'; // –
constexpr char32_t EM_DASH      = U'\u2014'; // —

// ==================== 2. Ellipsis ====================
constexpr char32_t ELLIPSIS = U'\u2026'; // …

// ==================== 3. Curly Quotes (Arabic / Latin) ====================
constexpr char32_t LEFT_SINGLE_QUOTE  = U'\u2018'; // ‘
constexpr char32_t RIGHT_SINGLE_QUOTE = U'\u2019'; // ’
constexpr char32_t LEFT_DOUBLE_QUOTE  = U'\u201C'; // “
constexpr char32_t RIGHT_DOUBLE_QUOTE = U'\u201D'; // ”
constexpr char32_t LEFT_ANGLE_QUOTE   = U'\u00AB'; // «
constexpr char32_t RIGHT_ANGLE_QUOTE  = U'\u00BB'; // »

// ==================== 4. Urdu‑Specific Symbols ====================
constexpr char32_t ARABIC_PERCENT_SIGN        = U'\u066A'; // ٪
constexpr char32_t ARABIC_ASTERISK            = U'\u066D'; // ٭
constexpr char32_t ARABIC_PER_MILLE_SIGN      = U'\u0609'; // ؉
constexpr char32_t ARABIC_PER_TEN_THOUSAND    = U'\u060A'; // ؊

// ==================== 5. Additional Latin Punctuation and Symbols ====================
constexpr char32_t NUMBER_SIGN     = U'\u0023'; // #
constexpr char32_t AMPERSAND       = U'\u0026'; // &
constexpr char32_t AT_SIGN         = U'\u0040'; // @
constexpr char32_t BACKSLASH       = U'\u005C'; //
constexpr char32_t CARET           = U'\u005E'; // ^
constexpr char32_t UNDERSCORE      = U'\u005F'; // _
constexpr char32_t GRAVE_ACCENT    = U'\u0060'; // `
constexpr char32_t VERTICAL_BAR    = U'\u007C'; // |
constexpr char32_t TILDE           = U'\u007E'; // ~

// ==================== 6. Brackets and Braces (additional) ====================
constexpr char32_t LEFT_SQUARE_BRACKET   = U'\u005B'; // [
constexpr char32_t RIGHT_SQUARE_BRACKET  = U'\u005D'; // ]
constexpr char32_t LEFT_CURLY_BRACE      = U'\u007B'; // {
constexpr char32_t RIGHT_CURLY_BRACE     = U'\u007D'; // }

// ==================== 7. Other Separators ====================
constexpr char32_t ARABIC_THOUSANDS_SEPARATOR   = U'\u066C'; // ٬
constexpr char32_t ARABIC_DECIMAL_SEPARATOR     = U'\u066B'; // ٫

constexpr char32_t TATWEEL_OR_KASHIDA = U'\u0640'; // A stretching character used for visual emphasis. It carries no linguistic meaning and should be stripped.
                                                   // It is already part of isUrduLetter(). 
                                                   // It should be part of ALL_PUNCTUATION set, so it optionally should not pass through the cleaner untouched and corrupt tokens.
                                                   // lib/normalization.md for more detail

// Optional: an array of all punctuation code points for iteration
constexpr char32_t ALL_PUNCTUATION[] = {
    EXCLAMATION_MARK,
    QUOTATION_MARK,
    LEFT_PARENTHESIS,
    RIGHT_PARENTHESIS,
    PLUS_SIGN,
    FULL_STOP_LATIN,
    SOLIDUS,
    COLON,
    ARABIC_COMMA,
    ARABIC_SEMICOLON,
    ARABIC_QUESTION_MARK,
    ARABIC_FULL_STOP,
    ZERO_WIDTH_NON_JOINER,
    /*ZERO_WIDTH_JOINER*/ // Not considering ZWJ as a candidate for removal
    HYPHEN_MINUS,
    EN_DASH,
    EM_DASH,
    ELLIPSIS,
    LEFT_SINGLE_QUOTE,
    RIGHT_SINGLE_QUOTE,
    LEFT_DOUBLE_QUOTE,
    RIGHT_DOUBLE_QUOTE,
    LEFT_ANGLE_QUOTE,
    RIGHT_ANGLE_QUOTE,
    ARABIC_PERCENT_SIGN,
    ARABIC_ASTERISK,
    ARABIC_PER_MILLE_SIGN,
    ARABIC_PER_TEN_THOUSAND,
    NUMBER_SIGN,
    AMPERSAND,
    AT_SIGN,
    BACKSLASH,
    CARET,
    UNDERSCORE,
    GRAVE_ACCENT,
    VERTICAL_BAR,
    TILDE,
    LEFT_SQUARE_BRACKET,
    RIGHT_SQUARE_BRACKET,
    LEFT_CURLY_BRACE,
    RIGHT_CURLY_BRACE,
    ARABIC_THOUSANDS_SEPARATOR,
    ARABIC_DECIMAL_SEPARATOR,
    TATWEEL_OR_KASHIDA, // Part of normalization process, this point code is already part of isUrduLetter(). For more dedtail please go through lib/normalization.md file
};

constexpr size_t NUM_PUNCTUATION_SYMBOLS = sizeof(ALL_PUNCTUATION) / sizeof(ALL_PUNCTUATION[0]);

} // namespace UrduPunctuation

#endif // CSV_PARSER_LIB_PUNCTUATION_SYMBOLS_HH