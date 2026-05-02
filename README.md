# Naqsh — Urdu Text Cleaner and Normalizer

[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)](https://isocpp.org/)
[![Encoding](https://img.shields.io/badge/encoding-UTF--8-green)]()
[![Status](https://img.shields.io/badge/status-under%20construction-orange)]()

---

## What is Naqsh?

**Naqsh** (نقش) is a C++ library for cleaning and normalizing Urdu text encoded in UTF-8.

Urdu presents unique challenges for text processing: a right-to-left script built on Arabic with its own extended characters, invisible formatting characters like the Zero-Width Non-Joiner, mixed-script content (Urdu alongside digits and Latin), and multiple Unicode representations of the same letter. Most general-purpose parsers ignore these entirely. Naqsh is built specifically around them.

Naqsh is **not** a standalone tokenizer or parser. It is designed to be used as an optional cleaning hook for the [Parser](https://github.com/KHAAdotPK/Parser.git) package — it cleans and normalizes each line before Parser's `Iterator` splits it into tokens. For English text, the equivalent package is [imprint](https://github.com/KHAAdotPK/imprint.git).

The name comes from the Urdu/Persian word نقش — meaning *pattern* or *imprint*.

---

## How It Integrates with Parser

Inside Parser's `Iterator.hh`, the cleaning step is guarded by a preprocessor macro:

```cpp
#ifdef ITERATOR_USER_DEFINED_CLEANER_CODE
    Cleaner cleaner;
#endif

if (std::getline(*_stream, line))
{
#ifdef ITERATOR_USER_DEFINED_CLEANER_CODE
    line = cleaner.cleanLine(line);
#endif
    // ... tokenize line ...
}
```

For `Cleaner` to be in scope when `Iterator.hh` compiles, **Naqsh must be included before Parser** in your entry point. The macro must also be defined before either header is pulled in.

```cpp
// In your main.hh or entry point

#define ITERATOR_USER_DEFINED_CLEANER_CODE   // activates cleaning in Iterator

#include "../lib/Naqsh/header.hh"            // brings Cleaner into scope
#include "../lib/Parser/header.hh"           // Iterator now sees Cleaner
```

If `ITERATOR_USER_DEFINED_CLEANER_CODE` is not defined, Parser works as normal and Naqsh is never referenced — there is no overhead or side effect.

---

## Usage

```cpp
// main.hh

#ifdef CSV_PARSER_TOKEN_DELIMITER
#undef CSV_PARSER_TOKEN_DELIMITER
#endif
#define CSV_PARSER_TOKEN_DELIMITER ' '

#ifndef ITERATOR_GUARD_AGAINST_EMPTY_STRING
#define ITERATOR_GUARD_AGAINST_EMPTY_STRING
#endif

#ifndef ITERATOR_USER_DEFINED_CLEANER_CODE
#define ITERATOR_USER_DEFINED_CLEANER_CODE
#endif

// Include order matters — Naqsh before Parser
#include "../lib/Hash/header.hh"
#include "../lib/Naqsh/header.hh"    // Cleaner must be in scope before Parser
#include "../lib/Parser/header.hh"
```

With this setup, every line read by Parser's `Iterator` is passed through Naqsh's `Cleaner::cleanLine()` before being split into tokens.

`cleanLine` can also be called directly if you need to clean a line independently:

```cpp
Cleaner cleaner;

std::string line = "اسلام علیکم! آج کا دن 10:30 بجے شروع ہوا۔";
std::string cleaned = cleaner.cleanLine(line);

// result: "اسلام علیکم آج کا دن 10:30 بجے شروع ہوا"
```

### Compiling

```bash
# Standard mode — Unicode §3.9 recovery (recommended)
g++ -std=c++17 -o my_program main.cpp

# Drop mode — discard entire invalid UTF-8 sequence
g++ -std=c++17 -DDROP_INVALID_UTF8_SEQUENCE -o my_program main.cpp

# Enable religious ligature normalization
g++ -std=c++17 -DNORMALIZE_RELIGIOUS_LIGATURES -o my_program main.cpp

# Custom token delimiter (default is comma)
g++ -std=c++17 -DCSV_PARSER_TOKEN_DELIMITER='\t' -o my_program main.cpp

# Combined example
g++ -std=c++17 -DNORMALIZE_RELIGIOUS_LIGATURES -DDROP_INVALID_UTF8_SEQUENCE -o my_program main.cpp
```

---

## Building the Vocabulary Table

Once Naqsh and Parser are wired together, a single call to `build_hash_table()` reads the entire corpus, cleans every line through `Cleaner::cleanLine()`, and returns a fully-built `TABLES*` containing:

- a hash table of every unique token (the vocabulary),
- a per-word linked list of every occurrence in the corpus, and
- a linked list of every line, each holding a linked list of its tokens in order.

```cpp
#include <iostream>
#include <fstream>

#define KEYS_COMMON_STARTING_SIZE 23
#define ITERATOR_USER_DEFINED_CLEANER_CODE
#define CSV_PARSER_TOKEN_DELIMITER ' '

#include "../lib/Naqsh/header.hh"
#include "../lib/Hash/header.hh"
#include "../lib/Parser/header.hh"

int main(int argc, char* argv[])
{
    try
    {
        Parser parser("../lib/Naqsh/data/test.txt");

        TABLES* tables = parser.build_hash_table();

        // ── Vocabulary statistics ─────────────────────────────────────────────
        std::cout << "Bucket Count:             " << tables->get_bucket_count() << "\n";
        std::cout << "Buckets Used (vocab size):" << tables->get_bucket_used()  << "\n";
        std::cout << "Maximum Tokens Per Line:  " << tables->get_maximum_tokens_per_line() << "\n";
        std::cout << "Minimum Tokens Per Line:  " << tables->get_minimum_tokens_per_line() << "\n";
        std::cout << "Total Tokens:             " << tables->get_total_tokens() << "\n";

        // ── Corpus reconstruction via the lines linked list ───────────────────
        WordRecord** hash_table  = tables->hash_to_word_record;
        size_t*      index_table = tables->word_id_to_hash;

        std::ofstream ofile("output.txt");

        LINE* current_line = tables->lines;         // head of the line linked list

        while (current_line != nullptr)
        {
            std::cout << "Line with " << current_line->n << " tokens: ";

            TOKEN* current_token = current_line->tokens; // head of this line's token list
            current_line = current_line->next;           // advance line cursor

            while (current_token != nullptr)
            {
                size_t token_id = current_token->token_id;          // word_id for this token
                std::cout << token_id << " ";

                // word_id  →  bucket index  →  WordRecord  →  word string
                WordRecord* word_record = hash_table[index_table[token_id]];
                ofile << word_record->word << " ";

                current_token = current_token->next;
            }

            ofile << "\n";
        }
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

### What the loop does

The `lines` linked list preserves the full sequential layout of the corpus — every line in order, every token within each line in order. Each `TOKEN` node carries two fields:

| Field | Type | Meaning |
|---|---|---|
| `token_id` | `size_t` | The word's permanent ID — its row index in the embedding matrix |
| `occurrence` | `OccurrenceNode*` | Pointer to this token's exact position record `(line, token)` |

To recover the word string from a `TOKEN`, follow the two-step lookup:

```
token_id  →  index_table[token_id]          →  bucket index
           →  hash_table[bucket_index]       →  WordRecord*
           →  word_record->word              →  std::string
```

This is O(1) — two array dereferences and a pointer follow.

### `KEYS_COMMON_STARTING_SIZE`

Controls the initial number of buckets in the hash table. It should be a prime number. The table rehashes automatically as the vocabulary grows, so the starting size only affects how soon the first rehash is triggered. For small corpora `23` is fine; for large corpora a larger prime (e.g. `1009`) reduces early rehash overhead.

---

## What it does

### Hand-rolled UTF-8 decoder

Naqsh decodes UTF-8 byte sequences manually, one code point at a time, without relying on any external library. It understands the full 1–4 byte structure and handles malformed input through one of two compile-time policies:

- **`DROP_INVALID_UTF8_SEQUENCE`** — the entire claimed sequence is discarded when any continuation byte is invalid. Aggressive but simple.
- **Default (no flag)** — follows Unicode §3.9 maximal-subpart recovery: only the lead byte is discarded, and the offending byte is re-examined as the start of a new sequence. This means a valid ASCII character immediately following a bad lead byte is never lost.

### Punctuation removal

A `static unordered_set<char32_t>` is built once from `PunctuationSymbols.hh` and used for O(1) lookup on every code point. The set covers:

- Core Urdu punctuation: `۔` `،` `؛` `؟`
- Latin punctuation: `.` `,` `:` `;` `!` `"` `(` `)` and more
- Hyphens, dashes, ellipsis, curly quotes, angle quotes
- Arabic-specific symbols: `٪` `٭` `؉` `؊`
- Tatweel/Kashida `ـ` (U+0640) — a stretching character used for visual emphasis with no linguistic meaning
- Additional separators and brackets

The set is defined in `PunctuationSymbols.hh` and is straightforward to extend.

### Zero-Width Non-Joiner (ZWNJ) handling

ZWNJ (`U+200C`) appears in Urdu text to control letter joining. Naqsh handles two cases:

- **Trailing ZWNJ** — silently removed
- **ZWNJ between two Urdu letters** — replaced with a space, correctly splitting the display compound into two tokens

```
خوب‌صورت  →  خوب صورت
```

Zero-Width Joiner (`U+200D`) is intentionally left untouched — it signals that two characters should render as one glyph and is considered part of the token.

### Operator preservation between digits

`:`, `+`, `.`, and `٫` are in the punctuation set and would normally be stripped. A state machine detects the pattern `digit operator digit` and re-emits the operator, preserving time values, arithmetic expressions, and decimal numbers.

```
10:30    →  10:30    colon preserved
۲+۳      →  ۲+۳      plus preserved
3.14     →  3.14     decimal dot preserved
3٫14     →  3٫14     Arabic decimal separator preserved
10+3+5   →  10+3+5   chained plus preserved
اردو:    →  اردو     colon not between digits, stripped
۱. آج    →  ۱ آج     ordinal dot not followed by digit, stripped
Rs. 500  →  Rs 500   abbreviation dot not followed by digit, stripped
```

All three digit systems are recognized: Western `0–9`, Eastern Arabic `٠–٩`, Urdu `۰–۹`.

### Dot preservation between alpha sequences

A second state machine detects the pattern `alpha DOT alpha` and preserves the dot, enabling correct handling of domain names and file extensions in mixed Urdu text.

```
urdu.com      →  urdu.com     alpha dot alpha, preserved
www.urdu.com  →  www.urdu.com chained dots, preserved
data.csv      →  data.csv     file extension, preserved
Rs.500        →  Rs 500       alpha dot digit, dropped
ver.2         →  ver 2        alpha dot digit, dropped
```

The two machines share a single `pendingDot` slot with an ownership flag (`dotSetByDigitMachine`) to prevent them from incorrectly claiming each other's dot.

### Unicode normalization

A `normalize` function maps visually identical but code-point-distinct characters to a single canonical form before they are written to the output. Called for every alpha character and every digit as each passes through the cleaner.

**Letter normalization** — Arabic keyboards and Urdu keyboards produce different code points for the same letter:

| Input | Canonical | Description |
|---|---|---|
| U+0643 `ك` | U+06A9 `ک` | Arabic kaf → Urdu keheh |
| U+064A `ي` | U+06CC `ی` | Arabic yeh → Farsi yeh |
| U+0647 `ه` | U+06C1 `ہ` | Arabic heh → heh goal |

**Hamza normalization** — multiple alif forms collapsed to plain alif:

| Input | Canonical | Description |
|---|---|---|
| U+0623 `أ` | U+0627 `ا` | Alif with hamza above |
| U+0625 `إ` | U+0627 `ا` | Alif with hamza below |
| U+0671 `ٱ` | U+0627 `ا` | Alif wasla |

U+0622 `آ` (Alif with madda) is intentionally excluded — the madda indicates a long vowel that changes the word's identity (`آج` "today", `آپ` "you"). Collapsing it to plain alif would produce non-existent words.

**Digit normalization** — Eastern Arabic digits and Urdu digits are visually similar but distinct code points. Arabic-layout keyboards emit the Eastern Arabic form while Urdu text conventionally uses the Urdu form:

| Input | Canonical |
|---|---|
| U+0660–U+0669 `٠١٢٣٤٥٦٧٨٩` | U+06F0–U+06F9 `۰۱۲۳۴۵۶۷۸۹` |

Digit normalization is applied inside the digit state machine so that operator preservation and normalization work correctly together — `٣٫١٤` becomes `۳٫۱۴` with the decimal separator intact.

### Religious ligature normalization

Arabic Presentation Forms (U+FB50–U+FDFF) contain precomposed ligatures for religious phrases and honorifics such as `ﷺ` (U+FDFA), `ﷻ` (U+FDFB), and `ﷲ` (U+FDF2). By default Naqsh passes these through untouched as single opaque tokens, since they are rare in general Urdu NLP corpora (news, literature, legal and government text).

When processing religious corpora where these ligatures appear frequently, enable expansion with:

```bash
g++ -std=c++17 -DNORMALIZE_RELIGIOUS_LIGATURES -o my_program main.cpp
```

See the comment block in `normalize()` for the full list of affected code points and their expansions.

### Whitespace normalization

`collapseSpace` runs as a post-processing step on the output of `cleanLine`. It collapses runs of consecutive whitespace characters to a single space and strips leading and trailing whitespace. Characters treated as whitespace:

- U+0020 space
- U+0009 tab
- U+00A0 non-breaking space
- U+061C Arabic letter mark

### UTF-8 encoder

`appendCodePoint` encodes any `char32_t` code point back to UTF-8 bytes and appends to a `std::string`. Used internally by the normalization path to emit the canonical form of a normalized character.

---

## File structure

```
Naqsh/
├── header.hh                  ←  top-level include, pulls in both libraries
└── lib/src
    ├── Cleaner.hh             ←  UTF-8 decoder, cleanLine, state machines,
    │                              normalization, collapseSpace
    └── PunctuationSymbols.hh  ←  Unicode code point definitions
```

---

## Compile-Time Macros

| Macro | Package | Effect |
|---|---|---|
| `ITERATOR_USER_DEFINED_CLEANER_CODE` | Parser / Naqsh | Activates `Cleaner::cleanLine()` inside `Iterator::read_next()` |
| `DROP_INVALID_UTF8_SEQUENCE` | Naqsh | Discard entire invalid UTF-8 sequence instead of single lead byte |
| `NORMALIZE_RELIGIOUS_LIGATURES` | Naqsh | Expands Arabic Presentation Forms ligatures (U+FB50–U+FDFF) to constituent letters. Off by default — enable only for religious corpora |
| `CSV_PARSER_TOKEN_DELIMITER` | Naqsh / Parser | Token delimiter character — defaults to `','` in Naqsh's `header.hh`; override before including if a different delimiter is needed |
| `ITERATOR_GUARD_AGAINST_EMPTY_STRING` | Parser | Skips empty tokens after splitting |

---

## Design decisions

| Decision | Reason |
|---|---|
| Hand-rolled UTF-8 decoder | Full control at the byte level, zero external dependencies |
| `char32_t` code points | Correct comparison for multi-byte Urdu characters |
| `static unordered_set` | Punctuation set built once, not per line |
| Compile-time recovery policy | Caller decides how aggressive invalid-byte handling should be |
| `NORMALIZE_RELIGIOUS_LIGATURES` off by default | Naqsh targets general Urdu NLP pipelines where these code points are rare |
| Exclusions before letter ranges in `isUrduLetter` | Guarantees non-letters are never accepted even if ranges are later widened |
| State machines for ZWNJ, colon, plus, and dot | These are contextual rules — a simple character lookup cannot handle them |
| `pendingDot` shared between digit and alpha machines | One ownership flag (`dotSetByDigitMachine`) makes the interaction explicit rather than relying on execution order |
| Digit normalization inside the digit state machine | Ensures operator preservation and normalization work in the correct order |
| `normalize` as a pure switch table | Adding a new normalization is a one-line change with no risk of affecting other code paths |
| U+0622 excluded from hamza normalization | Alif with madda is a distinct phoneme in Urdu — collapsing it to plain alif produces non-existent words |
| `collapseSpace` as a separate post-processing step | Keeps the main decode loop simple — whitespace normalization runs once on the finished string |

---

## Current limitations

- Method definitions live in `.hh` files. Including from multiple translation units will cause linker errors. This will be resolved when the library is split into `.hh`/`.cc` pairs.
- No dictionary, morphology, or grammar — Naqsh is a cleaner and normalizer, not an NLP framework.
- URL detection uses an alpha-dot-alpha heuristic. Full URL preservation (`https://urdu.com`) is a future improvement.
- Religious ligatures are passed through untouched by default. Enable `NORMALIZE_RELIGIOUS_LIGATURES` when processing religious corpora.

---

## Related Packages

| Package | Role |
|---|---|
| [Parser](https://github.com/KHAAdotPK/Parser.git) | CSV / text file iterator that Naqsh plugs into |
| [imprint](https://github.com/KHAAdotPK/imprint.git) | English text cleaner — Naqsh's counterpart for English corpora |

---

## Contributing

The project is under active development. Issues and pull requests are welcome. If you are working with Urdu text and have corpus samples that break the cleaner, opening an issue with the sample is the most useful contribution you can make right now.

---

## License

This project is governed by a license, the details of which can be located in the accompanying file named 'LICENSE.'

---

*Naqsh — built for Urdu, from the ground up.*
