# naqsh — Urdu Text Parser

---

## What is naqsh?

**naqsh** (نقش) is a C++ library for reading, cleaning, and tokenizing Urdu text encoded in UTF-8.

Urdu presents unique challenges for text processing: a right-to-left script built on Arabic with its own extended characters, invisible formatting characters like the Zero-Width Non-Joiner, mixed-script content (Urdu alongside digits and Latin), and multiple Unicode representations of the same letter. Most general-purpose parsers ignore these entirely. naqsh is built specifically around them.

The name comes from the Urdu/Persian word نقش — meaning *pattern* or *imprint*.

---

## What it does right now

### Hand-rolled UTF-8 decoder

naqsh decodes UTF-8 byte sequences manually, one code point at a time, without relying on any external library. It understands the full 1–4 byte structure and handles malformed input through one of two compile-time policies:

- **`DROP_INVALID_UTF8_SEQUENCE`** — the entire claimed sequence is discarded when any continuation byte is invalid. Aggressive but simple.
- **Default (no flag)** — follows Unicode §3.9 maximal-subpart recovery: only the lead byte is discarded, and the offending byte is re-examined as the start of a new sequence. This means a valid ASCII character immediately following a bad lead byte is never lost.

### Punctuation removal

A `static unordered_set<char32_t>` is built once from `PunctuationSymbols.hh` and used for O(1) lookup on every code point. The set covers:

- Core Urdu punctuation: `۔` `،` `؛` `؟`
- Latin punctuation: `.` `,` `:` `;` `!` `"` `(` `)` and more
- Hyphens, dashes, ellipsis, curly quotes, angle quotes
- Arabic-specific symbols: `٪` `٭` `؉` `؊`
- Additional separators and brackets

The set is defined in `PunctuationSymbols.hh` and is straightforward to extend.

### Zero-Width Non-Joiner (ZWNJ) handling

ZWNJ (`U+200C`) appears in Urdu text to control letter joining. naqsh handles two cases:

- **Trailing ZWNJ** — silently removed (it is also in the punctuation set, so removal is automatic)
- **ZWNJ between two Urdu letters** — replaced with a space, correctly splitting the display compound into two tokens

```
خوب‌صورت  →  خوب صورت
```

Zero-Width Joiner (`U+200D`) is intentionally left untouched — it signals that two characters should render as one glyph and is considered part of the token.

### Colon preservation between digits

Because `:` is in the punctuation set, it would normally be stripped. A small state machine detects the pattern `digit : digit` and re-emits the colon, preserving time values.

```
10:30  →  10:30      (preserved)
اردو:  →  اردو       (colon stripped)
```

All three digit systems are recognized: Western `0–9`, Eastern Arabic `٠–٩`, Urdu `۰–۹`.

### `isUrduLetter`

A function that determines whether a Unicode code point is an Urdu letter. Exclusions are checked first (diacritics, digits, tatweel, ZWNJ/ZWJ, whitespace), then explicit letter ranges are tested. Covers:

- Core Arabic block `U+0621–U+06FF` with precise range selection
- Arabic Supplement block `U+0750–U+077F`
- Explicitly excludes Latin letters, Eastern Arabic digits, Koranic diacritics, and tatweel

---

## File structure

```
naqsh/
├── header.hh                  ←  top-level include, pulls in both libraries
└── lib/
    ├── Parser.hh              ←  UTF-8 decoder, cleanLine, state machines
    └── PunctuationSymbols.hh  ←  Unicode code point definitions
```

---

## Usage

```cpp
#include "header.hh"

int main()
{
    Parser parser;

    std::string line = "اسلام علیکم! آج کا دن 10:30 بجے شروع ہوا۔";
    std::string cleaned = parser.cleanLine(line);

    // result: "اسلام علیکم آج کا دن 10:30 بجے شروع ہوا"

    return 0;
}
```

### Compiling

```bash
# Standard mode — Unicode §3.9 recovery (recommended)
g++ -std=c++17 -o my_program main.cpp

# Drop mode — discard entire invalid sequence
g++ -std=c++17 -DDROP_INVALID_UTF8_SEQUENCE -o my_program main.cpp

# Custom delimiter (default is comma)
g++ -std=c++17 -DCSV_PARSER_TOKEN_DELIMITER='\t' -o my_program main.cpp
```

---

## What naqsh wants to become

naqsh is currently a cleaner. The goal is a full Urdu tokenizer API where the caller can iterate over lines and tokens the same way they would in any modern NLP pipeline.

### Phase 1 — Tokenization
A `tokenize` method that returns a `std::vector<std::string>` of discrete tokens from a cleaned line. Consecutive spaces collapsed, empty tokens discarded, leading and trailing whitespace handled inside the library rather than pushed to the caller.

### Phase 2 — Normalization
Real Urdu text in the wild contains the same word in multiple Unicode representations. For example, `ک` can appear as U+06A9 (Urdu keheh) or U+0643 (Arabic kaf) — visually identical, different code points, different tokens without normalization. Phase 2 will address:

- Urdu keheh vs Arabic kaf
- Inconsistent hamza forms
- Eastern Arabic digits vs Urdu digits for the same numeral

### Phase 3 — Document and line API

```cpp
// Conceptual future interface
Document doc("urdu_corpus.txt");

for (auto& line : doc.lines()) {
    for (auto& token : line.tokens()) {
        std::cout << token.text()       << "\n";  // token text
        std::cout << token.byteOffset() << "\n";  // position in original
        std::cout << token.type()       << "\n";  // Urdu / Latin / Digit / Mixed
    }
}
```

### Phase 4 — Token metadata
- Diacritics (zabar, zer, pesh, shadda) separated from the base letter and accessible independently
- Token type classification: Urdu, Latin, Digit, Mixed, Punctuation
- Original byte offset preserved so tokens can be mapped back to the source text

---

## Design decisions

| Decision | Reason |
|---|---|
| Hand-rolled UTF-8 decoder | Full control at the byte level, zero external dependencies |
| `char32_t` code points | Correct comparison for multi-byte Urdu characters |
| `static unordered_set` | Punctuation set built once, not per line |
| Compile-time recovery policy | Caller decides how aggressive invalid-byte handling should be |
| Exclusions before letter ranges in `isUrduLetter` | Guarantees non-letters are never accepted even if ranges are later widened |
| State machines for ZWNJ and colon | These are contextual rules — they cannot be handled with a simple character lookup |

---

## Current limitations

- `cleanLine` returns a cleaned `std::string` — it does not yet return a vector of tokens. The caller must still split on spaces.
- No normalization yet — the same word can produce two different strings depending on which Unicode representation was used.
- Method definitions live in `.hh` files. Including from multiple translation units will cause linker errors. This will be resolved when the library is split into `.hh`/`.cc` pairs.
- No dictionary, morphology, or grammar — naqsh is a tokenizer, not an NLP framework.

---

## Contributing

The project is under active development. Issues and pull requests are welcome. If you are working with Urdu text and have corpus samples that break the tokenizer, opening an issue with the sample is the most useful contribution you can make right now.

---

## License



---

*naqsh — built for Urdu, from the ground up.*
