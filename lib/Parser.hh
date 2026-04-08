/*
    lib/Parser.hh
    Q@hackers.pk
 */

/*
    The /DDROP_INVALID_UTF8_SEQUENCE is a compile time flag
    -------------------------------------------------------
    A deliberate policy choice, treat the whole claimed sequence as garbage and discard it.
    The len is already set to the number of bytes in the sequence... 
    The /DDROP_INVALID_UTF8_SEQUENCE skips the entire invalid sequence.

    Need for ifndef /DDROP_INVALID_UTF8_SEQUENCE
    --------------------------------------------
    Consider this specific byte sequence in memory: [0xE0] [0x41] [0x80]
    - 0xE0 → 3-byte lead byte, so len = 3
    - 0x41 → not a valid continuation byte (must start with 10xx xxxx)
    - 0x80 → valid continuation byte

    Without /DDROP_INVALID_UTF8_SEQUENCE:
    - Inner loop runs for j = 1 and j = 2
    - On j = 1, 0x41 is invalid → inner loop breaks
    - i increments by 1 (only the 0xE0 byte)
    - Next iteration starts at 0x41, which is treated as a new sequence
    - Result: 0xE0 is dropped, 0x41 is treated as a separate character

    With /DDROP_INVALID_UTF8_SEQUENCE:
    - Inner loop runs for j = 1 and j = 2
    - On j = 1, 0x41 is invalid (but a valid ASCII 'A') so validUTF8Sequence = false
    - Inner loop breaks
    - Outer loop increments i by len (which is 3)
    - Entire [0xE0] [0x41] [0x80] sequence is skipped
    - Next iteration starts after the invalid sequence

    Conclusion
    ----------
    So the policy of "skip the whole declared length" is only safe if you trust that the bytes following the bad lead byte genuinely belong to the broken sequence.
    But a bad lead byte followed immediately by a valid ASCII byte is a real-world corruption pattern, and your code discards the ASCII byte.
 */ 

#ifndef CSV_PARSER_LIB_PARSER_HH
#define CSV_PARSER_LIB_PARSER_HH

/*
    UTF‑8 decoding process, which follows a most‑significant‑first (big‑endian) order for the logical code point value
    That is most significant bits are stored at lower memory addresses.
 */

class Parser 
{
    public:
        Parser(void);
        ~Parser(void);

        std::string cleanLine(const std::string&); // Clean the line by removing punctuation and other characters
        bool isDigit(char32_t); // Check if the code point is a digit
        bool isTokenDelimiter(char32_t); // Check if the code point is a token delimiter
        bool isUrduLetter(char32_t); // Check if the code point is an Urdu letter        
};

Parser::Parser(void)
{    
}

Parser::~Parser(void)
{    
}

std::string Parser::cleanLine(const std::string& line)
{
    bool skip = false;
//#ifdef DROP_INVALID_UTF8_SEQUENCE    
    bool validUTF8Sequence = true; // All sequences are valid untill proven otherwise
//#endif    

    // Variables for state machine logic for colon between two digits as in 10:45
    bool isDigitFound = false, isColonFound = false;
    // Variables for state machine logic for Zero Width Non-Joiner
    bool isZeroWidthNonJoinerFound = false;
    
    std::string result;
    result.reserve(line.size());

    // Store punctuation code points in a set for O(1) lookup
    static const std::unordered_set<char32_t> punctSet(
        std::begin(UrduPunctuation::ALL_PUNCTUATION),
        std::end(UrduPunctuation::ALL_PUNCTUATION)
    );

    size_t i = 0;
    while (i < line.size())
    {
        // Decode one UTF-8 code point
        /*
            Decode one UTF-8 code point. Code Point is the unique number assigned to each character
            The unique numbe can be multibyte entity. The first byte of the multibyte entity
            determines the number of bytes in the entity and this byt also contains the first few bits of the code point (unique number)
            The "cp" has the first few bits of the code point (unique number) and the remaining bits are extracted from the subsequent bytes.
            The "len" has the number of bytes in the entity.
         */
        char32_t cp = 0;
        unsigned char c = line[i];
        size_t len = 0;

        /*
            CASE 1 When macro DROP_INVALID_UTF8_SEQUENCE is defined
            --------------------------------------------------------
            The following masks are used to determine the number of bytes in a UTF-8 sequence
            and the number of bits to extract from each byte.

            Once number UTF-8 code point is determined, 
            then any octet that is not a valid subsequent byte makes the entire UTF-8 sequence invalid.

            The following masks are used to extract the remaining bits from the subsequent bytes
            1-byte ASCII, 7-bit non-extended ASCII [ 01111111 ]
            2-byte, 11-bit [ 10xxxxxx 110xxxxx ]
            3-byte, 16-bit [ 10xxxxxx 10xxxxxx 1110xxxx ]
            4-byte, 21-bit [ 10xxxxxx 10xxxxxx 10xxxxxx 11110xxx ]
         */
         /*
            CASE 2 When macro DROP_INVALID_UTF8_SEQUENCE is not defined
            ------------------------------------------------------------
            The following masks are used to extract the remaining bits from the subsequent bytes
            Then the remining octetes are read UTF08 octet at a time.
            If the subsequent octet is not a valid UTF-8 octet, then fo the error octet and the subsequent octets the inner loop is prematurely broken
            and the i is incremented by the valid octets count. In this way no subsequent octet is dropped.

            1-byte ASCII, 7-bit non-extended ASCII [ 01111111 ]
            2-byte, 11-bit [ 10xxxxxx 110xxxxx ]
            3-byte, 16-bit [ 10xxxxxx 10xxxxxx 1110xxxx ]
            4-byte, 21-bit [ 10xxxxxx 10xxxxxx 10xxxxxx 11110xxx ]
          */
        if ((c & 0x80) == 0x00)  // 1-byte ASCII, 7-bit non-extended ASCII [ 01111111 ]
        {
            cp = c; // Code Point, this is a single byte entity and it has no more remaining bits to be extracted from the subsequent bytes
            len = 1;
        }
        else if ((c & 0xE0) == 0xC0) // 2-byte, 11-bit [ 10xxxxxx 110xxxxx ]
        { 
            cp = c & 0x1F; // Code Point, this is a two byte entity and it has 5 remaining bits to be extracted from this single byte and remaining bits from the subsequent byte
            len = 2;
        }
        else if ((c & 0xF0) == 0xE0) // 3-byte, 16-bit [ 10xxxxxx 10xxxxxx 1110xxxx ]
        { 
            cp = c & 0x0F; // Code Point, this is a three byte entity and it has 4 remaining bits to be extracted from this single byte and remaining bits from the subsequent two bytes
            len = 3;
        } 
        else if ((c & 0xF8) == 0xF0) // 4-byte, 21-bit [ 10xxxxxx 10xxxxxx 10xxxxxx 11110xxx ]
        { 
            cp = c & 0x07; // Code Point, this is a four byte entity and it has 3 remaining bits to be extracted from this single byte and remaining bits from the subsequent three bytes
            len = 4;
        } 
        else
        {
            // Invalid UTF-8, skip this byte
            ++i;
            continue;
        }

        // (when len i s 1) The following loop does not get executed
        // (When len is more than 1) Starting from the next byte, cp has the first few bits of the code point
        // The remaining bits are extracted from the subsequent bytes and appended to the code point
        // The << 6 shifts the code point to the left by 6 bits, making room for the next 6 bits
        // The & 0x3F masks the subsequent byte to extract the lower 6 bits.

        // If a cp is not valid UTF-8 sequence, then all subsequent bytes are also invalid len many bytes are also invalid
        for (size_t j = 1; j < len; ++j)
        {
            if (i  + j >= line.size() || (line[i + j] & 0xC0) != 0x80)
            {
                // Invalid sequence, break out
#ifdef DROP_INVALID_UTF8_SEQUENCE   
                /*
                    The state machines lose state across invalid sequences                    
                    ------------------------------------------------------
                    If an invalid UTF-8 byte appears between a digit and :, validUTF8Sequence is false so the state machine doesn't run, but isDigitFound is not reset.
                    The state leaks across the invalid byte. Same for isZeroWidthNonJoinerFound.
                    
                    Since part of the UTF-8 sequence is invalid, we need to skip the entire sequence
                    The len is already set to the number of bytes in the sequence
                    At the end of the outer loop, the i variable is incremented by len, which skips the entire invalid sequence
                 */
                isColonFound = false;
                isDigitFound = false;
                isZeroWidthNonJoinerFound = false;
                validUTF8Sequence = false;
                /*
                    Since part of the UTF-8 sequence is invalid, we need to skip the entire sequence
                    The len is already set to the number of bytes in the sequence
                    At the end of the outer loop, the i variable is incremented by len, which skips the entire invalid sequence
                 */
#endif

#ifndef DROP_INVALID_UTF8_SEQUENCE  
                /*
                    Recovery policy: only skip bytes up to (not including) the
                    first byte that failed the continuation check.

                    A UTF-8 lead byte declares how many bytes belong to its
                    sequence (len). If one of those continuation bytes turns out
                    to be invalid, we have two choices:

                    (A) Skip all len bytes  -- the original implicit behaviour.
                        Simple, but destructive: the byte at position j that
                        FAILED the check may itself be a valid lead byte or a
                        valid ASCII character. Skipping it silently discards
                        data that was never part of the broken sequence.

                        Example:  [0xE0] [0x41] [0x80]
                                  lead    'A'   continuation
                        Option A skips all three, 'A' is lost.

                    (B) Skip only j bytes (i.e. set len = j)  -- chosen here.
                        We discard only the lead byte and the continuation bytes
                        we have already consumed up to the point of failure.
                        The offending byte at position j is left untouched so
                        the outer loop re-examines it as the start of a new
                        sequence on the very next iteration.

                        Same example with option B:
                            [0xE0] [0x41] [0x80]
                        Option B skips only [0xE0], then re-examines [0x41]
                        which decodes correctly as ASCII 'A'.

                    This is the standard "maximal subpart" recovery recommended
                    by Unicode 15 §3.9 (Conformance): replace only the shortest
                    ill-formed subsequence, then resume decoding from the next
                    byte. Setting len = j implements exactly that boundary.
                */                                
                len = j; 

                /*
                    The state machines lose state across invalid sequences                    
                    ------------------------------------------------------
                    Here we break out of the inner loop and validUTF8Sequence is never declared, so all the state machine blocks run unconditionally on the partially decoded cp.
                    But cp at this point holds a garbage partial value (only the bits accumulated before j was hit). The state machines (ZWNJ, colon, isDigit etc.) will then operate on a meaningless code point.
                    We should reset all state flags in the #ifndef path too, just as we did in the #ifdef path:
                 */
                isColonFound = false;
                isDigitFound = false;
                isZeroWidthNonJoinerFound = false;

                /*
                   The append block at the bottom should also be guarded in this path.
                   Currently without validUTF8Sequence, it will always append, including for this truncated invalid sequence.
                   We need a separate flag (or reuse a local bool like skip/validUTF8Sequence) to suppress the append in this #ifndef path when len was shortened due to invalidity.
                 */
                validUTF8Sequence = false;                
#endif                               
                break; // Break out of the for loop in both cases (#ifdef DROP_INVALID_UTF8_SEQUENCE and #ifndef DROP_INVALID_UTF8_SEQUENCE)
            }
            // We are moving from little-endina to big-endina?
            cp = (cp << 6) | (line[i + j] & 0x3F); // Upto j (exclusive) bytes have been processed.
            // The remaining bits are extracted from the subsequent bytes and appended to the code point
            // The << 6 shifts the code point to the left by 6 bits, making room for the next 6 bits
            // The & 0x3F masks the subsequent byte to extract the lower 6 bits.
            // utf-8 is big-endian, THAT IS MOST SIGNIFICANT BYTE FIRST OR MOST-SIGNIFICANT BITS stored at lower memory addresses.
            // Intel x86 is little-endian, THAT IS LEAST SIGNIFICANT BYTE FIRST OR LEAST-SIGNIFICANT BITS stored at lower memory addresses.  
            // This is why we need to shift the code point to the left by 6 bits to make room for the next 6 bits
            // and then mask the subsequent byte to extract the lower 6 bits.

        } // End of for loop
        /**********************************************************************************
        //               Deal with the token delimiter starts here                       //
        /*********************************************************************************/
//#ifdef DROP_INVALID_UTF8_SEQUENCE        
        if (validUTF8Sequence)
//#endif            
        {
            if (isTokenDelimiter(cp))
            {                
                skip = true; // The delimeter is not part of puntuations/letters/symbols to be skipped.
                             // So are asking the append logic explicitly to skip it.

                result += ' ';            
            }
        }                        
        /***********************************************************************************
        //               Deal with the token delimiter ends here                          //
        /**********************************************************************************/


        /*
            The following logic handles the case where ZWNJ is followed by an Urdu letter.
            In this case, the ZWNJ is replaced by a space.
         */
        /*********************************************************************************
        //          The state machine logic start here for Zero Width Non-Joiner        //
        /*********************************************************************************/
//#ifdef DROP_INVALID_UTF8_SEQUENCE        
        if (validUTF8Sequence)
//#endif            
        {   
            /*
                ZWNJ is part of ALL_PUNCTUATION aeeay. 
                After following statement sets the ZWNJ flag, the ZWNJ is removed automatically.
                That takes care of trailing ZWNJ, and ZWNj any where else (which is damgerou).                                
             */         
            if (cp == UrduPunctuation::ZERO_WIDTH_NON_JOINER)
            {
                isZeroWidthNonJoinerFound = true;

                // Right after this the ZWNJ gets removed automatically becuase ZWNJ is included in ALL_PUNCTUATION
                // This will deal with the ZWNJ at the end of the token/word. 
            }                        
            else if (isZeroWidthNonJoinerFound) // In the next iteration this will cover ZWNJ between two Urdu letters and it gets replaced by a space, the ZWJN got removed automatically in the previous iteration.
            {
                isZeroWidthNonJoinerFound = false; // Reset the flag

                // Check if next character is Urdu letter and nothing else.

                if (isUrduLetter(cp)) // If the next character is Urdu letter, then insert space before it.
                {
                    result += ' ';   
                }

                // Then fall through to normal processing for cp
            }
        }
        /*********************************************************************************
        //          The state machine logic ends here for Zero Width Non-Joiner         //
        /**********************************************************************************/

        /*
            The following logic handles the case where colon is between two digits as in 10:45
            Becuase colon is in ALL_PUNCTUATION array, it gets removed automatically.
            So we need to handle this case separately.
         */
        /**********************************************************************************
        // The state machine logic starts here for colon between two digits as in 10:45  //
        // Check if cp is a digit                                                        //
        **********************************************************************************/
//#ifdef DROP_INVALID_UTF8_SEQUENCE        
        if (validUTF8Sequence)
//#endif            
        {
            if (isDigit(cp))
            {
                if (isColonFound) // If the previous character was a colon and this cp is didgit append a colon before appending this didit
                {
                    /*
                        Since U+003A is in the ASCII range (0x00–0x7F), UTF-8 encodes it as a single byte 0x3A — exactly what ':' is.
                        There is no difference at the byte level in a UTF-8 string.
                        The only time you'd need multi-byte encoding is for code points above U+007F, i.e., non‑ASCII characters.
                        For example if you wanted to re-emit an Arabic comma U+060C you'd ...
                        ```C++
                            // U+060C in UTF-8 is 2 bytes: 0xD8 0x8C
                            result += '\xD8';
                            result += '\x8C';
                        ```
                    */
                    result += ':';
                    isColonFound = false;
                }
                else
                {
                    isDigitFound = true; // In first(and every subsequent) iteration, if the character is a digit, set the flag to true
                }
            }
            else if (isDigitFound) // In this iteration cp is not digit, but the previous character was a digit. So we need to check if the current character is a colon 
            {
                if (cp == U'\u003A') // If this code point (cp) is colon , reset isDigitFound and set isColonFound
                {
                    isDigitFound = false;
                    isColonFound = true;
                }
                else // If this code point (cp) is not colon , reset isDigitFound
                {
                    isDigitFound = false; 
                }
            }
            else if (isColonFound) // In this iteration cp is not digit, and the previous character was not a digit but isColonFound is true (this is the case when colon is not followed by a digit)
            {
                isColonFound = false; // Reset the flag
            }
        }
        /*********************************************************************************
        // The state machine logic ends here for colon between two digits as in 10:45   //
        *********************************************************************************/
//#ifdef DROP_INVALID_UTF8_SEQUENCE        
        if (validUTF8Sequence)
//#endif            
        {
            if (punctSet.find(cp) == punctSet.end()) // If this code point is NOT punctuation, append the original bytes
            {
                if (!skip)
                {
                    result.append(line, i, len);
                }
            }                        
        }

        i += len; // Read notes at the top of this file for more information about the value of len

        skip = false; // skip is only valid for the iteration in which it is set
//#ifdef DROP_INVALID_UTF8_SEQUENCE        
        validUTF8Sequence = true; // All sequences are valid untill proven otherwise
//#endif            
                
    } // End of while loop
    return result;
}


bool Parser::isDigit(char32_t cp)
{
    return (cp >= U'\u0030' && cp <= U'\u0039') || (cp >= U'\u0660' && cp <= U'\u0669') || (cp >= U'\u06F0' && cp <= U'\u06F9');
}

/**
 * Checks if a given Unicode code point is a token delimiter.
 * 
 * A token delimiter is a character that separates tokens in the input string.
 * This function returns true if the code point is equal to the token delimiter
 * defined by the CSV_PARSER_TOKEN_DELIMITER macro.
 * 
 * @param cp The Unicode code point to check
 * @return true if the code point is a token delimiter, false otherwise
 */
bool Parser::isTokenDelimiter(char32_t cp)
{
    return cp == CSV_PARSER_TOKEN_DELIMITER;
}

/**
 * Determines whether a given Unicode code point represents an Urdu letter.
 * 
 * Urdu script is based on the Arabic script with additional characters.
 * This function returns true only for characters that are considered letters
 * (i.e., consonants, vowels, and independent vowel carriers) in Urdu text.
 * 
 * It explicitly excludes:
 *   - Diacritical marks (e.g., zabar, zer, pesh, shadda)
 *   - Digits (Western and Eastern Arabic)
 *   - Punctuation (both Latin and Arabic)
 *   - Whitespace and control characters
 *   - Zero-width joiners/non-joiners
 *   - Ligature presentation forms that are not base letters
 * 
 * @param cp Unicode code point (32-bit value)
 * @return true if cp is an Urdu letter, false otherwise
 */
bool Parser::isUrduLetter(char32_t cp)
{
    // ============================================================
    // 1. PRIMARY URDU LETTERS (Arabic block, U+0600–U+06FF)
    // ============================================================
    // This block contains the core Arabic alphabet used for Urdu.
    // However, not everything in this range is a letter:
    //   - U+0600–U+060B: formatting characters, signs (NOT letters)
    //   - U+060C–U+060F: punctuation (comma, semicolon, etc.)
    //   - U+0610–U+061A: diacritics (marks above/below letters)
    //   - U+061B–U+061F: punctuation (semicolon, question mark, etc.)
    //   - U+0620–U+063F: Arabic letters (includes many Urdu letters)
    //   - U+0640: tatweel/kashida (stretching character – NOT a letter)
    //   - U+0641–U+064A: more Arabic letters (includes Urdu letters)
    //   - U+064B–U+065F: diacritical marks (NOT letters)
    //   - U+0660–U+0669: Eastern Arabic digits (NOT letters)
    //   - U+066A–U+066D: punctuation (NOT letters)
    //   - U+066E–U+066F: Arabic letter (two characters, actually letters)
    //   - U+0670: diacritic (not letter)
    //   - U+0671–U+06D3: Arabic letters (most are used in Urdu)
    //   - U+06D4: Arabic full stop (punctuation)
    //   - U+06D5: Arabic letter (used in Urdu: "ے" - yeh barree)
    //   - U+06D6–U+06ED: diacritics and Koranic marks (NOT letters)
    //   - U+06EE–U+06EF: Arabic letters (used in some Urdu contexts)
    //   - U+06F0–U+06F9: Eastern Arabic digits (NOT letters)
    //   - U+06FA–U+06FF: additional Arabic letters (some used in Urdu)
    //
    // For simplicity and safety, we define explicit letter ranges
    // based on common Urdu practice. These ranges cover:
    //   - All Urdu alphabet characters (alif to yeh barree)
    //   - Additional letters like "ڑ" (U+0691), "ڈ" (U+0688), "ژ" (U+0698)
    //   - The 'heh doachashmee' (U+06BE) and 'heh goal' (U+06C1)
    //   - The 'noon ghunna' (U+06BA)
    //   - The 'yeh barree' (U+06D2, U+06D5)
    // 
    // Ranges:
    //   U+0621–U+0628: Hamza, Alif, Alif with madda, Alif with hamza,
    //                  Waw with hamza, Alif with hamza below, Yeh with hamza,
    //                  Alef maksura (not standard Urdu but kept)
    //   U+062A–U+063A: Teh, Theh, Jeem, Hah, Khah, Dal, Thal, Reh, Zain,
    //                  Seen, Sheen, Sad, Dad, Tah, Zah, Ain, Ghain
    //   U+0641–U+064A: Feh, Qaf, Kaf, Lam, Meem, Noon, Heh, Waw, Yeh (alif maksura)
    //   U+066E–U+066F: Arabic letter 'Damma isolated'? Actually these are
    //                  'dotless beh' and 'dotless qaf' – rare but keep as letters
    //   U+0671–U+0671: Alif with wasla (sometimes used)
    //   U+0679–U+0681: Tteh, Tteh with dot, etc. (Urdu letters: ٹ, ٺ, ٻ, etc.)
    //   U+0683–U+0687: Urdu letters: ڃ, ڄ, چ, ڇ (cheh, etc.)
    //   U+0688–U+0698: Urdu letters: ڈ, ډ, ڊ, ڋ, ڌ, ڍ, ڎ, ڏ, ڐ, ڑ, ڒ, ړ, ڔ, ڕ, ږ, ژ
    //   U+069A–U+06A0: more Urdu letters: ښ, ڛ, ڜ, ڝ, ڞ, ڟ, ڠ, ڡ, ڢ, ڣ
    //   U+06A1–U+06A8: ڡ, ڢ, ڣ, ڤ, ڥ, ڦ, ڧ, ڨ
    //   U+06A9–U+06AF: ک, ڪ, ګ, ڬ, ڭ, ڮ, گ (keheh, gaf)
    //   U+06B0–U+06B3: ڰ, ڱ, ڲ, ڳ (gaaf, etc.)
    //   U+06BA–U+06BA: noon ghunna (ں)
    //   U+06BB–U+06BF: ڻ, ڼ, ڽ, ھ (heh doachashmee)
    //   U+06C0–U+06C3: ۀ, ہ, ۂ, ۃ (heh goal, etc.)
    //   U+06C4–U+06C4: ۄ (waw with dot)
    //   U+06C5–U+06C6: ۅ, ۆ (various waw)
    //   U+06C7–U+06C9: ۇ, ۈ, ۉ (various waw)
    //   U+06CA–U+06CE: ۊ, ۋ, ی (yeh barree)
    //   U+06D0–U+06D0: ې (yeh barree with dot)
    //   U+06D2–U+06D3: ے, ۓ (yeh barree and its hamza form)
    //   U+06D5: ے (alternate yeh barree)
    //   U+06EE–U+06EF: ۮ, ۯ (rare)
    //   U+06FA–U+06FF: ۺ, ۻ, ۼ, ۽, ۾, ۿ (rare)
    //
    // For a practical Urdu tokenizer, we can simplify by taking:
    //   Arabic letters minus diacritics, minus digits, minus punctuation.
    //   But safer to use explicit ranges based on common Urdu letters.
    
    // ============================================================
    // 1. EXCLUSIONS – check these FIRST (fixes unreachable logic)
    // ============================================================
    // Even if a code point would later match a letter range, we explicitly
    // exclude:
    //   - Diacritics (U+064B–U+065F, U+0670, U+06D6–U+06ED, etc.)
    //   - Digits (U+0660–U+0669, U+06F0–U+06F9)
    //   - Tatweel/Kashida (U+0640)
    //   - Zero-width joiners/non-joiners (U+200C, U+200D)
    //   - Whitespace (space, tab, newline)
    //
    // Placing this block FIRST guarantees that non-letters are never
    // accidentally accepted, even if a later letter range is widened.
    if ( (cp >= 0x064B && cp <= 0x065F) ||  // Arabic diacritics
         (cp == 0x0670) ||                  // Arabic superscript alef
         (cp >= 0x06D6 && cp <= 0x06ED) ||  // Koranic diacritics
         (cp >= 0x0660 && cp <= 0x0669) ||  // Eastern Arabic digits
         (cp >= 0x06F0 && cp <= 0x06F9) ||  // Eastern Arabic digits (alternate)
         (cp == 0x0640) ||                  // Tatweel (stretching character)
         (cp == 0x200C) ||                  // ZWNJ – zero width non-joiner
         (cp == 0x200D) ||                  // ZWJ  – zero width joiner
         (cp == 0x0020) ||                  // space
         (cp == 0x0009) ||                  // tab
         (cp == 0x000A) )                   // newline
    {
        return false;
    }

    // ============================================================
    // 2. PRIMARY URDU LETTERS (Arabic block, U+0600–U+06FF)
    // ============================================================
    // (Detailed comments from original code preserved.)
    // These ranges cover all standard Urdu alphabet characters.
    if ( (cp >= 0x0621 && cp <= 0x0628) ||  // Hamza, Alif, etc.
         (cp >= 0x062A && cp <= 0x063A) ||  // Teh to Ghain
         (cp >= 0x0641 && cp <= 0x064A) ||  // Feh to Yeh
         (cp >= 0x066E && cp <= 0x066F) ||  // Dotless Beh, Dotless Qaf
         (cp >= 0x0671 && cp <= 0x0671) ||  // Alif with wasla
         (cp >= 0x0679 && cp <= 0x0681) ||  // Tteh, etc.
         (cp >= 0x0683 && cp <= 0x0687) ||  // Cheh, etc.
         (cp >= 0x0688 && cp <= 0x0698) ||  // Dal, Reh, etc. including ژ
         (cp >= 0x069A && cp <= 0x06A0) ||  // ښ to ڠ
         (cp >= 0x06A1 && cp <= 0x06A8) ||  // ڤ to ڨ
         (cp >= 0x06A9 && cp <= 0x06AF) ||  // ک to گ
         (cp >= 0x06B0 && cp <= 0x06B3) ||  // ڰ to ڳ
         (cp == 0x06BA) ||                  // noon ghunna
         (cp >= 0x06BB && cp <= 0x06BF) ||  // ڻ to ھ
         (cp >= 0x06C0 && cp <= 0x06C3) ||  // ۀ to ۃ
         (cp == 0x06C4) ||                  // ۄ
         (cp >= 0x06C5 && cp <= 0x06C9) ||  // ۅ to ۉ
         (cp >= 0x06CA && cp <= 0x06CE) ||  // ۊ to ی
         (cp == 0x06D0) ||                  // ې
         (cp >= 0x06D2 && cp <= 0x06D3) ||  // ے and ۓ
         (cp == 0x06D5) ||                  // ے (alt)
         (cp >= 0x06EE && cp <= 0x06EF) ||  // ۮ, ۯ
         (cp >= 0x06FA && cp <= 0x06FF) )   // rare characters
    {
        return true;
    }

    // ============================================================
    // 3. ARABIC PRESENTATION FORMS (Ligatures)
    // ============================================================
    // Ligatures like U+FDFA (ﷺ) are not individual letters.
    // Return false (or customize if you need to treat them as units).
    // (No change from original – exclusions already handled above.)

    // ============================================================
    // 4. ARABIC SUPPLEMENT (U+0750–U+077F)
    // ============================================================
    // Some rare Urdu characters may appear here. The earlier exclusion
    // block already filtered out any diacritics or digits in this range,
    // so we can safely return true for the whole block.
    if (cp >= 0x0750 && cp <= 0x077F) {
        return true;
    }

    // ============================================================
    // 5. LATIN LETTERS (not Urdu letters)
    // ============================================================
    if ( (cp >= 0x0041 && cp <= 0x005A) ||  // A-Z
         (cp >= 0x0061 && cp <= 0x007A) )   // a-z
    {
        return false;
    }

    // ============================================================
    // 6. DEFAULT
    // ============================================================
    return false;
}

#endif // CSV_PARSER_LIB_PARSER_HH