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

        void appendCodePoint(std::string&, char32_t); // Append the code point to the result
        std::string cleanLine(const std::string&); // Clean the line by removing punctuation and other characters
        std::string collapseSpace(const std::string&); // 
        bool isAlpha(char32_t); // Check if the code point is an alphabet
        bool isDigit(char32_t); // Check if the code point is a digit
        bool isDot(char32_t); // Check if the code point is a dot
        bool isTokenDelimiter(char32_t); // Check if the code point is a token delimiter
        bool isUrduLetter(char32_t); // Check if the code point is an Urdu letter 
        char32_t normalize(char32_t); // Normalize the code point       
};

Parser::Parser(void)
{    
}

Parser::~Parser(void)
{    
}

void Parser::appendCodePoint(std::string& result, char32_t cp)
{
    if (cp <= 0x7F) // 1-byte ASCII, 7-bit non-extended ASCII [ 01111111 ]
    {
        result += (char)cp; // It's just a single byte and it's endianness doesn't matter
    }
    else if (cp <= 0x7FF) // 2-byte, 11-bit [ 10xxxxxx 110xxxxx ] 
    {
        result += (char)(0xC0 | (cp >> 6)); // 110xxxxx // Most significant bits to least significant bits 
        result += (char)(0x80 | (cp & 0x3F)); // 10xxxxxx // Least significant bits to most significant bits 
    }
    else if (cp <= 0xFFFF) // 3-byte, 16-bit [ 10xxxxxx 10xxxxxx 1110xxxx ]
    {
        result += (char)(0xE0 | (cp >> 12)); // 1110xxxx // Most significant bits to least significant bits 
        result += (char)(0x80 | ((cp >> 6) & 0x3F)); // 10xxxxxx // Least significant bits to most significant bits 
        result += (char)(0x80 | (cp & 0x3F)); // 10xxxxxx // Least significant bits to most significant bits 
    }
    else // 4-byte, 21-bit [ 10xxxxxx 10xxxxxx 10xxxxxx 11110xxx ]
    {
        result += (char)(0xF0 | (cp >> 18)); // 11110xxx // Most significant bits to least significant bits 
        result += (char)(0x80 | ((cp >> 12) & 0x3F)); // 10xxxxxx // Least significant bits to most significant bits 
        result += (char)(0x80 | ((cp >> 6) & 0x3F)); // 10xxxxxx // Least significant bits to most significant bits 
        result += (char)(0x80 | (cp & 0x3F)); // 10xxxxxx // Least significant bits to most significant bits 
    }
}

std::string Parser::cleanLine(const std::string& line)
{
    bool skip = false;
    bool validUTF8Sequence = true; // All sequences are valid untill proven otherwise

    // Variables for state machine logic for colon between two digits as in 10:45
    bool isDigitFound = false, isColonFound = false;
    // Variables for state machine logic for Zero Width Non-Joiner
    bool isZeroWidthNonJoinerFound = false;
    // Variables for state machine logic for plus operator
    bool isPlusOperatorFound = false;
    // Variables for state machine logic for dot
    char32_t pendingDot = 0; // 0 means no dot pending or more precisely the U+002E or U+066B if digit was seen; 0 otherwise
    bool dotSetByDigitMachine = false; // This flag is set when a dot is encountered after a digit
    // Variables for state machine logic for alpha-dot-alpha
    bool isAlphaFound = false;
    
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
                isPlusOperatorFound = false;
                isDigitFound = false;
                isZeroWidthNonJoinerFound = false;
                validUTF8Sequence = false;

                isAlphaFound = false;
                pendingDot = 0;
                dotSetByDigitMachine = false;
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
                isPlusOperatorFound = false;
                isDigitFound = false;
                isZeroWidthNonJoinerFound = false;
                isAlphaFound = false;
                pendingDot = 0;
                dotSetByDigitMachine = false;

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
        //               State machine to handle aphabets starts here                    //
        /*********************************************************************************/
        if (validUTF8Sequence)
        {
            if (isAlpha(cp)) // If the current character is an alphabet
            { 
                if (pendingDot != 0) // If the previous character was a dot
                {
                    if (!dotSetByDigitMachine) // If the dot was not set by the digit machine
                    {                                            
                        // U+002E is ASCII, single byte, emit directly
                        // U+066B is 2-byte UTF-8: 0xD9 0xAB
                        if (pendingDot == UrduPunctuation::FULL_STOP_LATIN)
                        {
                            result += '.';
                        }
                        else if (pendingDot == UrduPunctuation::ARABIC_DECIMAL_SEPARATOR)
                        {
                            result += '\xD9';
                            result += '\xAB';
                        }
                        pendingDot = 0;
                    }
                }

                char32_t normalized = normalize(cp);

                if (normalized != cp)
                {
                    // emit normalized UTF-8 bytes
                    appendCodePoint(result, normalized);
                    skip = true;
                }

                isAlphaFound = true;            
            }
            else if (isAlphaFound) // If the previous iteration was an Alphabet
            {
                if (isDot(cp))
                {
                    pendingDot = cp;
                }
                
                isAlphaFound = false;
            }
        }   
        /**********************************************************************************
        //               State machine to handle aphabets ends here                      //
        /*********************************************************************************/

        /**********************************************************************************
        //               Deal with the token delimiter starts here                       //
        /*********************************************************************************/
        if (validUTF8Sequence)
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
        if (validUTF8Sequence)
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
        /**********************************************************************************
        // The state machine logic starts here for colon, plus, and dot between two      //
        // digits as in 10:45, 10+3, or 3.14. Check if cp is a digit                    //
        **********************************************************************************/
        /*
            State machine: preserve operator between two digits
            ===================================================
            Handles colon (U+003A), plus (U+002B), and dot/decimal separator
            (U+002E, U+066B) that appear between two digit characters. All three
            are in ALL_PUNCTUATION and would normally be stripped. This state
            machine detects the digit-operator-digit pattern and re-emits the
            operator before the second digit is appended.

            Digit sets recognized:
                Western Arabic   0–9     (U+0030–U+0039)
                Eastern Arabic   ٠–٩     (U+0660–U+0669)
                Urdu             ۰–۹     (U+06F0–U+06F9)

            Cases handled:

                10:30           →  10:30       colon between two digits, preserved
                10+3            →  10+3        plus between two digits, preserved
                3.14            →  3.14        dot between two digits, preserved
                3٫14            →  3٫14        Arabic decimal separator, preserved
                10+3+5          →  10+3+5      chained plus, all preserved
                10:30:45        →  10:30:45    chained colons, all preserved
                3.14+2          →  3.14+2      mixed operators, all preserved
                ۲+۳             →  ۲+۳         Urdu digits with plus, preserved
                ۱۰:۳۰           →  ۱۰:۳۰       Urdu digits with colon, preserved

                ۱. آج           →  ۱ آج        ordinal dot not followed by digit, dropped
                اردو:           →  اردو        colon not preceded by digit, dropped
                :30             →  30          colon not preceded by digit, dropped
                10:             →  10          colon not followed by digit, dropped
                10:abc          →  10 abc      colon not followed by digit, dropped
                10:+5           →  105         chained operators — both stripped, digits
                                               concatenate directly. No space is inserted
                                               because the cleaner only re-emits operators,
                                               it never synthesizes separators. This is
                                               acceptable: digit-operator-operator sequences
                                               do not occur in real Urdu text.
                3.+2            →  32          same behaviour as 10:+5 above.
                +30             →  30          plus not preceded by digit, dropped
                10+             →  10          plus not followed by digit, dropped
                .30             →  30          dot not preceded by digit, dropped

                urdu.com        →  urdu.com     dot between two alpha sequences, preserved
                www.urdu.com    →  www.urdu.com chained alpha dots, all preserved
                data.csv        →  data.csv     file extension dot, preserved
                Rs. 500         →  Rs 500       dot after alpha then space, dropped
                Rs.500          →  Rs 500       dot after alpha, digit follows, dropped
                ver.2           →  ver 2        dot after alpha, digit follows, dropped
                version1.x      →  version1x    dot after digit, alpha follows, dropped

            Flags:
                isDigitFound         — last character processed was a digit
                isColonFound         — digit was seen, then colon; waiting for next digit
                isPlusOperatorFound  — digit was seen, then plus; waiting for next digit                `
                pendingDot           — U+002E or U+066B, set by either the digit machine
                                     (after digit) or the alpha machine (after alpha);
                                     0 means no dot pending
                dotSetByDigitMachine — true when pendingDot was set by the digit machine,
                                     false when set by the alpha machine. Used to decide
                                     whether a digit or alpha arriving next should
                                     re-emit or discard the dot.

            Invariant: isColonFound, isPlusOperatorFound, and pendingDot are mutually
            exclusive within the digit machine. pendingDot is additionally shared with
            the alpha machine — dotSetByDigitMachine records ownership.

            Re-emit order in the isDigit branch: pendingDot is checked first, then
            isPlusOperatorFound, then isColonFound. Order does not affect correctness
            since the flags are mutually exclusive, but dot is listed first as it is
            the most recently added operator.
        */
        if (validUTF8Sequence)
        {
            if (isDigit(cp)) // If the current character is a digit
            {                                   
                /*
                    Only re-emit pendingDot if dotSetByDigitMachine is true
                 */ 
                if (pendingDot != 0 && dotSetByDigitMachine) // If the previous character was a dot and it was set by the digit machine
                {
                    // U+002E is ASCII, single byte, emit directly
                    // U+066B is 2-byte UTF-8: 0xD9 0xAB
                    if (pendingDot == UrduPunctuation::FULL_STOP_LATIN)
                    {
                        result += '.';
                    }
                    else if (pendingDot == UrduPunctuation::ARABIC_DECIMAL_SEPARATOR)
                    {
                        result += '\xD9';
                        result += '\xAB';
                    }
                    pendingDot = 0;
                    dotSetByDigitMachine = false;
                }                
                else if (isPlusOperatorFound) // If the previous character was a plus operator and this cp is didgit append a plus operator before appending this didit
                {
                    result += '+'; // Append the plus operator
                    isPlusOperatorFound = false; // Reset the flag
                }
                else if (isColonFound) // If the previous character was a colon and this cp is didgit append a colon before appending this didit
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
                /*
                    When dotSetByDigitMachine is false (alpha machine set it), a digit arriving should clear pendingDot without re-emitting:
                 */
                else if (pendingDot != 0 && !dotSetByDigitMachine) // If the previous character was a dot and it was not set by the digit machine
                {
                    pendingDot = 0; // alpha-set dot not followed by alpha, discard
                }
                
                /*
                    Set unconditionally, covers both the normal digit case and the
                    case where an operator was just re-emitted and cp is still a digit.
                */
                isDigitFound = true; // In first(and every subsequent) iteration, if the character is a digit, set the flag to true                
            }
            else if (isDigitFound) // In this iteration cp is not digit, but the previous character was a digit. So we need to check if the current character is a colon 
            {
                if (cp == U'\u002B') // If this code point (cp) is plus operator , reset isDigitFound and set isPlusOperatorFound
                {                
                    isPlusOperatorFound = true;
                }
                else if (cp == U'\u003A') // If this code point (cp) is colon , reset isDigitFound and set isColonFound
                {                 
                    isColonFound = true;
                }
                else if (isDot(cp)) // If this code point (cp) is dot , reset isDigitFound and set pendingDot
                {                                     
                    pendingDot = cp; // Store the dot
                    dotSetByDigitMachine = true; // Set the flag
                }
                                
                // Set unconditionally, whether cp was an operator (colon/plus) or
                // any other non-digit character, isDigitFound must clear. The operator
                // flags carry the state forward if needed.
                isDigitFound = false;                 
            }
            else 
            {                
                /*
                    Neither a digit nor a character following a digit.
                    Any operator flag that was set (colon, plus, or dot) but never
                    followed by a digit is discarded here. The operator was not between
                    two digits so it does not belong in the output.
                    All flags are reset unconditionally because they are mutually
                    exclusive by construction, so resetting the already-false ones
                    is always a harmless no-op.
                */
                isColonFound = false; // Reset the flag. This is the case when colon is not followed by a digit
                isPlusOperatorFound = false; // Reset the flag. This is the case when plus operator is not followed by a digit
                if (dotSetByDigitMachine)
                {
                    pendingDot = 0; // Reset the pending dot
                    dotSetByDigitMachine = false; // Reset the flag
                }
                else if (!isAlpha(cp) && !isDot(cp))    
                {
                    // Dot was set by alpha machine but next meaningful character
                    // is not alpha — the dot is dangling, discard it.
                    pendingDot = 0;
                }
            }
        }
        /***********************************************************************************************************
        // The state machine logic ends here for colon, plus, and dot between two digits as in 10:45, 10+3, 3.14 //
        ***********************************************************************************************************/       
        if (validUTF8Sequence)
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
        validUTF8Sequence = true; // All sequences are valid untill proven otherwise
                
    } // End of while loop

    /* 
        Step 4. of Normalization. Multiple consecutive spaces, tabs mixed with spaces, non-breaking space U+00A0, and Arabic letter mark U+061C all need to collapse to a single space.
        Post-processing step on the result string of cleanLine()
     */   
    return collapseSpace (result);
}

/**
 * Collapses runs of whitespace in a UTF-8 string to a single space
 * and strips leading and trailing whitespace.
 *
 * This is a post-processing step applied to the output of cleanLine().
 * By the time this function runs, punctuation has been removed and
 * operators have been re-emitted, so the only remaining cleanup needed
 * is whitespace normalization.
 *
 * Four code points are treated as whitespace:
 *
 *   U+0020  SPACE              — standard ASCII space
 *   U+0009  TAB                — horizontal tab
 *   U+00A0  NO-BREAK SPACE     — non-breaking space (UTF-8: 0xC2 0xA0)
 *                                common in copy-pasted web text
 *   U+061C  ARABIC LETTER MARK — invisible RTL/LTR directional mark
 *                                (UTF-8: 0xD8 0x9C), appears in some
 *                                Urdu editors and word processors
 *
 * Behaviour:
 *   - Any run of one or more whitespace code points is replaced by a
 *     single ASCII space U+0020
 *   - Leading whitespace is stripped entirely (the !collapsed.empty()
 *     guard suppresses the space emission before the first non-whitespace
 *     character)
 *   - Trailing whitespace is stripped entirely (inWhitespace being true
 *     at end-of-string means nothing was appended for that final run)
 *   - Invalid UTF-8 bytes are skipped silently, consistent with the
 *     policy of the main decoder in cleanLine()
 *
 * Examples:
 *   "  Hello   World\t"  →  "Hello World"
 *   "\tیہ   اردو   ہے"   →  "یہ اردو ہے"
 *   "urdu.com  "         →  "urdu.com"
 *
 * @param result  UTF-8 encoded string, typically the output of cleanLine()
 * @return        A new string with whitespace runs collapsed and
 *                leading/trailing whitespace removed
 */
std::string Parser::collapseSpace(const std::string& result) 
{
    /*
        Post-processing: collapse whitespace runs
        =========================================
        Multiple consecutive spaces, tabs, non-breaking spaces (U+00A0),
        and Arabic letter marks (U+061C) are collapsed to a single space.
        Leading and trailing whitespace is stripped.

        All four characters are treated as whitespace for this purpose:
            U+0020  SPACE
            U+0009  TAB
            U+00A0  NO-BREAK SPACE        (UTF-8: 0xC2 0xA0)
            U+061C  ARABIC LETTER MARK    (UTF-8: 0xD8 0x9C)
     */
    
    std::string collapsed;
    collapsed.reserve(result.size());
    bool inWhitespace = false;

    size_t k = 0;
    while (k < result.size())
    {
        // Decode one UTF-8 code point from result
        char32_t cp2 = 0;
        unsigned char b = result[k];
        size_t seqLen = 0;

        if ((b & 0x80) == 0x00)      { cp2 = b;        seqLen = 1; }
        else if ((b & 0xE0) == 0xC0) { cp2 = b & 0x1F; seqLen = 2; }
        else if ((b & 0xF0) == 0xE0) { cp2 = b & 0x0F; seqLen = 3; }
        else if ((b & 0xF8) == 0xF0) { cp2 = b & 0x07; seqLen = 4; }
        else                         { ++k; continue; } // invalid byte, skip

        for (size_t m = 1; m < seqLen; ++m)
        {
            if (k + m >= result.size()) { seqLen = m; break; }
            cp2 = (cp2 << 6) | (result[k + m] & 0x3F);
        }

        // Check if this code point is whitespace
        bool isWS = (cp2 == 0x0020 ||   // space
                     cp2 == 0x0009 ||   // tab
                     cp2 == 0x00A0 ||   // non-breaking space
                     cp2 == 0x061C);    // Arabic letter mark

        if (isWS)
        {
            inWhitespace = true;
        }
        else
        {
            if (inWhitespace && !collapsed.empty())
            {
                collapsed += ' ';   // emit single space for the whole run
            }
            inWhitespace = false;
            collapsed.append(result, k, seqLen);
        }

        k += seqLen;
    }

    return std::move(collapsed);
}

/**
 * Determines whether a Unicode code point is an alphabetic character
 * for the purpose of the alpha-dot-alpha state machine.
 *
 * Recognizes two script groups:
 *
 *   Latin     A–Z  (U+0041–U+005A)
 *             a–z  (U+0061–U+007A)
 *
 *   Urdu/Arabic     all code points accepted by isUrduLetter()
 *                   see isUrduLetter() for the full range list
 *
 * This function is used to detect the alpha DOT alpha pattern that
 * preserves dots in domain names and file extensions (urdu.com, data.csv).
 * It intentionally excludes digits, diacritics, punctuation, and
 * whitespace — those are handled by separate functions.
 *
 * Mixed-script dot preservation (e.g. Urdu word DOT Latin word) follows
 * the same rule: if isALPHA returns true on both sides of the dot,
 * the dot is preserved regardless of which script each side belongs to.
 *
 * @param cp  Unicode code point (32-bit value)
 * @return    true if cp is a Latin or Urdu/Arabic letter, false otherwise
 */
bool Parser::isAlpha(char32_t cp)
{
    if ((cp >= U'A' && cp <= U'Z') || (cp >= U'a' && cp <= U'z'))
        return true;

    return isUrduLetter(cp);
}


/**
 * Determines whether a Unicode code point is a decimal digit.
 *
 * Three digit systems appear in Urdu text and all three are recognized:
 *
 *   Western Arabic   U+0030–U+0039   0 1 2 3 4 5 6 7 8 9
 *   Eastern Arabic   U+0660–U+0669   ٠ ١ ٢ ٣ ٤ ٥ ٦ ٧ ٨ ٩
 *   Urdu (Extended)  U+06F0–U+06F9   ۰ ۱ ۲ ۳ ۴ ۵ ۶ ۷ ۸ ۹
 *
 * All three ranges are used by the digit-operator state machine to detect
 * the digit-operator-digit pattern for colon (10:30), plus (۲+۳), and
 * dot (3.14). A digit from any of the three systems on either side of an
 * operator is sufficient to trigger preservation.
 *
 * Excluded intentionally:
 *   - Arabic-Indic digits in other blocks (rare, not standard in Urdu)
 *   - Superscript and subscript digits (U+2070–U+2079)
 *   - Enclosed digits (U+2460 and similar)
 *   - Full-width digits (U+FF10–U+FF19)
 *   These do not appear in standard Urdu corpus text and are treated
 *   as non-digits by this function.
 *
 * @param cp  Unicode code point (32-bit value)
 * @return    true if cp is a decimal digit in any of the three recognized
 *            systems, false otherwise
 */
bool Parser::isDigit(char32_t cp)
{
    return (cp >= U'\u0030' && cp <= U'\u0039') || (cp >= U'\u0660' && cp <= U'\u0669') || (cp >= U'\u06F0' && cp <= U'\u06F9');
}

/*
    isDot — recognizes code points that function as a decimal separator
    or domain/path separator in mixed Urdu text.

    Included:
        U+002E  FULL STOP LATIN          — decimal, URL, file extension
        U+066B  ARABIC DECIMAL SEPARATOR — Urdu decimal notation (٫)

    Excluded intentionally:
        U+06D4  ARABIC FULL STOP (۔)     — Urdu sentence terminator, purely punctuation, never a separator between meaningful tokens.
        U+FE52  SMALL FULL STOP          — legacy compatibility character, rare in modern Urdu text, add here if corpus evidence warrants it.
        U+FF0E  FULLWIDTH FULL STOP      — legacy compatibility character,
                                           same reasoning as U+FE52.
*/
bool Parser::isDot(char32_t cp)
{
    return cp == UrduPunctuation::FULL_STOP_LATIN || cp == UrduPunctuation::ARABIC_DECIMAL_SEPARATOR;
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

/*
 * Returns the normalized code point for a given Urdu/Arabic letter.
 * If no normalization is needed, returns cp unchanged.
 *
 * Normalizations applied:
 *   U+0643  Arabic kaf      →  U+06A9  Urdu keheh
 *   U+064A  Arabic yeh      →  U+06CC  Farsi yeh
 *   U+0647  Arabic heh      →  U+06C1  heh goal
 *   U+0623  Alif + hamza    →  U+0627  plain alif
 *   U+0625  Alif + hamza    →  U+0627  plain alif
 *   //U+0622  Alif + madda    →  U+0627  plain alif
 *   U+0671  Alif wasla      →  U+0627  plain alif
 */
char32_t Parser::normalize(char32_t cp)
{
    switch (cp)
    {
        /*
            1. Unicode character normalization for the same Urdu letter
            Arabic keyboards and Urdu keyboards produce different code points for visually identical letters. Real corpus text has both
         */
        case 0x0643: return 0x06A9;  // Arabic kaf     → Urdu keheh
        case 0x064A: return 0x06CC;  // Arabic yeh     → Farsi yeh
        case 0x0647: return 0x06C1;  // Arabic heh     → heh goal

        /*
            2. Hamza normalization
         */
        case 0x0623: return 0x0627;  // Alif + hamza above → plain alif
        case 0x0625: return 0x0627;  // Alif + hamza below → plain alif
        /* 
            U+0622 is Alif with madda, the madda is part of the word's pronunciation. The madda indicates a long vowel sound that changes the word's identity
            The normalization for U+0622 as 0x0627 is too aggressive
         */    
        // case 0x0622: return 0x0627;  // Alif + madda   → plain alif 
        case 0x0671: return 0x0627;  // Alif wasla     → plain alif
        default:     return cp;      // no normalization needed

        /*
            3. Tatweel / Kashida removal. This takes place through ALL_PUNCTUATION array
         */
        
        /*
            4. Whitespace normalization, method collapseSpace() handles this
         */         
    }
}

#endif // CSV_PARSER_LIB_PARSER_HH