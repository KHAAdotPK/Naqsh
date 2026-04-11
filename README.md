# naqsh — Urdu Text Parser

[![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)](https://isocpp.org/)
[![Encoding](https://img.shields.io/badge/encoding-UTF--8-green)]()
[![Status](https://img.shields.io/badge/status-under%20construction-orange)]()

---

## What is naqsh?

**naqsh** (نقش) is a C++ library for reading, cleaning, normalizing, and tokenizing Urdu text encoded in UTF-8.

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
- Tatweel/Kashida `ـ` (U+0640) — a stretching character used for visual emphasis with no linguistic meaning
- Additional separators and brackets

The set is defined in `PunctuationSymbols.hh` and is straightforward to extend.

### Zero-Width Non-Joiner (ZWNJ) handling

ZWNJ (`U+200C`) appears in Urdu text to control letter joining. naqsh handles two cases:

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

A `normalize` function maps visually identical but code-point-distinct characters to a single canonical form before they are written to the output. Applied to every alpha character as it passes through the cleaner.

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
| U+0622 `آ` | U+0627 `ا` | Alif with madda |
| U+0671 `ٱ` | U+0627 `ا` | Alif wasla |

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
naqsh/
├── header.hh                  ←  top-level include, pulls in both libraries
└── lib/
    ├── Parser.hh              ←  UTF-8 decoder, cleanLine, state machines,
    │                              normalization, collapseSpace
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

naqsh is currently a cleaner and normalizer. The goal is a full Urdu tokenizer API where the caller can iterate over lines and tokens the same way they would in any modern NLP pipeline.

### Phase 1 — Tokenization
A `tokenize` method that returns a `std::vector<std::string>` of discrete tokens from a cleaned line. Consecutive spaces collapsed, empty tokens discarded, leading and trailing whitespace handled inside the library rather than pushed to the caller.

### Phase 2 — Extended normalization
- Eastern Arabic digits vs Urdu digits for the same numeral
- URL and domain name detection to preserve full URLs as single tokens rather than relying on the alpha-dot-alpha heuristic
- Diacritic stripping as an optional mode — zabar, zer, pesh, shadda removed or preserved depending on caller preference

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
- Diacritics separated from the base letter and accessible independently
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
| State machines for ZWNJ, colon, plus, and dot | These are contextual rules — a simple character lookup cannot handle them |
| `pendingDot` shared between digit and alpha machines | One ownership flag (`dotSetByDigitMachine`) makes the interaction explicit rather than relying on execution order |
| `normalize` as a pure switch table | Adding a new normalization is a one-line change with no risk of affecting other code paths |
| `collapseSpace` as a separate post-processing step | Keeps the main decode loop simple — whitespace normalization runs once on the finished string |

---

## Current limitations

- `cleanLine` returns a cleaned `std::string` — it does not yet return a vector of tokens. The caller must still split on spaces.
- Method definitions live in `.hh` files. Including from multiple translation units will cause linker errors. This will be resolved when the library is split into `.hh`/`.cc` pairs.
- No dictionary, morphology, or grammar — naqsh is a tokenizer, not an NLP framework.
- URL detection uses an alpha-dot-alpha heuristic. Full URL preservation (`https://urdu.com`) is deferred to Phase 2.

---

## Contributing

The project is under active development. Issues and pull requests are welcome. If you are working with Urdu text and have corpus samples that break the tokenizer, opening an issue with the sample is the most useful contribution you can make right now.

---

## License

This project is governed by a license, the details of which can be located in the accompanying file named 'LICENSE.'

---

*naqsh — built for Urdu, from the ground up.*
