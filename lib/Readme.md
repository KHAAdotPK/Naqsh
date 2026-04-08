```C++
/*
 * ============================================================================
 * UTF‑8 DECODING – EDGE CASES & UNICODE STANDARD REQUIREMENTS
 * ============================================================================
 *
 * The current implementation handles basic UTF‑8 decoding but does NOT fully
 * enforce all validity checks required by the Unicode Standard (Chapter 3).
 * For production use, consider adding the following checks.
 *
 * 1. OVERLONG ENCODINGS
 *    - An overlong encoding uses more bytes than necessary for a code point.
 *      Example: ASCII 'A' (U+0041) encoded as 0xC1 0x81 (two bytes) is invalid.
 *    - Unicode Standard (Section 3.9): "Overlong UTF‑8 sequences are not
 *      permitted." They must be rejected to avoid security vulnerabilities
 *      (e.g., bypassing input filters).
 *    - Fix: After decoding, verify that the number of bytes used is the
 *      minimum required:
 *        - 1 byte  : cp < 0x80
 *        - 2 bytes : cp >= 0x80   and cp < 0x800
 *        - 3 bytes : cp >= 0x800  and cp < 0x10000
 *        - 4 bytes : cp >= 0x10000 and cp <= 0x10FFFF
 *
 * 2. INVALID CONTINUATION BYTES
 *    - Continuation bytes must have the pattern 10xxxxxx (0x80 – 0xBF).
 *    - If a continuation byte is missing or has the wrong pattern, the
 *      entire multibyte sequence is invalid.
 *    - Standard approach: Skip only the first byte of the invalid sequence
 *      and resynchronise at the next byte. Do NOT discard following bytes
 *      that may start a valid character.
 *    - Current code does this correctly (advances i by 1 on error).
 *
 * 3. CODE POINT RANGE (Maximum)
 *    - Unicode code points are limited to U+10FFFF (1,114,111 decimal).
 *    - Values above this (e.g., from 4‑byte sequences with first byte 0xF4..0xF7)
 *      are illegal and must be rejected.
 *
 * 4. REPLACEMENT CHARACTER (U+FFFD)
 *    - The Unicode Standard recommends replacing invalid sequences with
 *      the replacement character � (U+FFFD) rather than silently skipping them.
 *    - This preserves the length of the text and helps debugging.
 *    - If you choose to emit U+FFFD, append the UTF‑8 encoding of U+FFFD
 *      (three bytes: 0xEF 0xBF 0xBD) to the result string.
 *
 * 5. ADDITIONAL RECOMMENDATIONS
 *    - Never decode a code point that is a surrogate (U+D800 – U+DFFF).
 *      Surrogates are only valid in UTF‑16, not in UTF‑8.
 *    - Never decode a code point that is a non‑character (e.g., U+FFFE, U+FFFF,
 *      U+1FFFE, etc.). Non‑characters are reserved for internal use.
 *
 * Example of a corrected decoder snippet (inline replacement):
 *
 *   char32_t cp = 0;
 *   unsigned char c = line[i];
 *   size_t len = 0;
 *   bool valid = true;
 *
 *   // Determine length and initial bits
 *   if ((c & 0x80) == 0) { cp = c; len = 1; }
 *   else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; if (cp < 0x02) valid = false; }
 *   else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; if (cp == 0x00) valid = false; }
 *   else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; if (cp > 0x04) valid = false; }
 *   else { ++i; continue; }
 *
 *   // Read continuation bytes
 *   for (size_t j = 1; j < len && valid; ++j) {
 *       if (i + j >= line.size() || (line[i + j] & 0xC0) != 0x80) {
 *           valid = false;
 *           break;
 *       }
 *       cp = (cp << 6) | (line[i + j] & 0x3F);
 *   }
 *
 *   // Post‑decoding checks
 *   if (valid) {
 *       if (len == 2 && cp < 0x80) valid = false;
 *       if (len == 3 && cp < 0x800) valid = false;
 *       if (len == 4 && cp < 0x10000) valid = false;
 *       if (cp > 0x10FFFF) valid = false;
 *       if (cp >= 0xD800 && cp <= 0xDFFF) valid = false; // surrogates
 *   }
 *
 *   if (!valid) {
 *       // Option 1: skip the first byte
 *       ++i;
 *       continue;
 *       // Option 2: emit replacement character (U+FFFD) and skip one byte
 *       // result.append("\xEF\xBF\xBD");
 *       // ++i;
 *       // continue;
 *   }
 *
 * ============================================================================
 */
```