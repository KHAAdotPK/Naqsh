## Must Fix Before Tokenizer

**1. Unicode character normalization for the same Urdu letter**

This is the most important one. The same letter can exist as two different code points:

- `ک` — U+06A9 (Urdu keheh) vs U+0643 (Arabic kaf)
- `ی` — U+06CC (Farsi yeh) vs U+064A (Arabic yeh)
- `ہ` — U+06C1 (heh goal) vs U+0647 (Arabic heh)

Without normalization, `کتاب` written with U+06A9 and `کتاب` written with U+0643 are two different strings. Your tokenizer will produce two different tokens for the same word. Every dictionary lookup, frequency count, and downstream model will be wrong.

This is not a small edge case — Arabic keyboards and Urdu keyboards produce different code points for visually identical letters. Real corpus text has both.

---

**2. Hamza normalization**

Alif with hamza forms:
- `أ` U+0623, `إ` U+0625, `آ` U+0622, `ٱ` U+0671 — all variants of alif
- In standard Urdu orthography these are often written simply as `ا` U+0627

Without normalizing these, `اسلام` and `إسلام` are different tokens.

---

**3. Tatweel / Kashida removal**

`ـ` U+0640 is a stretching character used for visual emphasis — `کتاااب` instead of `کتاب`. It carries no linguistic meaning and should be stripped. You already exclude it from `isUrduLetter` but it is not in your `ALL_PUNCTUATION` set, so it currently passes through the cleaner untouched and will corrupt tokens.

Check your `PunctuationSymbols.hh` — if U+0640 is not in `ALL_PUNCTUATION`, add it.

---

**4. Whitespace normalization**

Multiple consecutive spaces, tabs mixed with spaces, non-breaking space U+00A0, and Arabic letter mark U+061C all need to collapse to a single space. Your cleaner currently passes spaces through as-is. The tokenizer splitting on spaces will produce empty tokens if two spaces appear together.

This can be a post-processing step on the result string of `cleanLine` — collapse runs of whitespace to a single space and strip leading/trailing whitespace.

---

## Can Wait Until After Tokenizer

**5. Eastern Arabic vs Urdu digit normalization**

`٣` U+0663 and `۳` U+06F3 are visually similar but different code points. For a tokenizer that treats them as digit tokens they will be separate token types. This matters more for downstream NLP than for tokenization itself.

**6. Ligature decomposition**

`ﷺ` U+FDFA and similar ligatures. These are single code points that represent entire phrases. A tokenizer will treat them as one token which is arguably correct. Decomposing them is an advanced normalization step.

**7. Diacritic handling**

Zabar, zer, pesh, shadda — whether to strip them or keep them as part of the token is a linguistic decision depending on your use case. For most Urdu NLP tasks they are stripped. But the tokenizer can produce the token either way and let the caller decide.

---

## Practical Order

Do 1, 2, 3, and 4 before tokenization. They are all simple character-level substitutions that fit naturally into `cleanLine`. Items 1 and 2 are a lookup table of about 10 entries. Item 3 is one line. Item 4 is a post-processing trim on the result string.

Without these four the tokenizer will produce inconsistent tokens from text that is linguistically identical, which undermines everything built on top of it.