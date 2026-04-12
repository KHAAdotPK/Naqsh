# C++ Naming Conventions

This document outlines the naming conventions to be used in this project for consistent and readable code.

## Classes
- Use **PascalCase** (also known as UpperCamelCase), where each word starts with an uppercase letter.
- **Example:** `class PlayerManager`, `class TextureLoader`

## Properties (Data Members)

### Public Properties
- Use **snake_case** (all lowercase, words separated by underscores).
- **Example:** `int player_score;`, `std::string user_name;`
- *Note:* Prefer using getters/setters instead of direct public access; if public members are necessary, follow this convention.

### Private Properties
- Use **\_snake_case** (snake_case with a leading underscore).
- **Example:** `int _health;`, `std::string _name;`
- The leading underscore clearly distinguishes private members from local variables and parameters.

## Methods (Member Functions)

### Public Methods
- Use **camelCase** (starts with a lowercase letter, each subsequent word capitalized).
- **Example:** `void updatePosition();`, `int getScore() const;`, `void setVisible(bool visible);`

### Private Methods
- Use **\_camelCase** (camelCase with a leading underscore).
- **Example:** `void _internalHelper();`, `bool _validateInput();`
- The underscore indicates that the method is intended for internal use only.

## Constants
- Use **UPPER_CASE** with underscores separating words.
- **Example:** `const int MAX_PLAYERS = 10;`, `const double PI = 3.14159;`

---

*Maintain consistency throughout the codebase. When in doubt, refer to this guide.*