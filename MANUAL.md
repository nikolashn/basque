# Current features of the Basque programming language
## Comments
Single-line comments use a `#` symbol; multi-line comments begin and end with `##`, and do not nest.
```
# Single line comment
write "b a s q u e\n"; # Another single line comment
##
Multiline
comment
##
```
## Types
Currently only string (string literal), i64 (64-bit signed integer) and u64 (64-bit unsigned integer) exist.
## Statements
`write <expression>;`

Outputs an expression to standard output (converting numeric expressions 64 bit unsigned integers and then to decimal strings of numbers)
```
write "Hello world!\n"; # Hello world!
write -1; # 18446744073709551615
```

`<expression>;`

Does nothing generally, unless the expression is made up of string literals, in which case it is like a write statement.
```
5+2; # Does nothing
"yo\n"; # yo
```

`<type> <identifier>` [` = <expression>`] `;`

Types currently are only `i64` and `u64`. An identifier begins with a letter and then may contain a series of letters, numbers and underscores. Identifiers are always initialized to 0.
```
u64 col = 0xfcf4d0u;
i64 _1234567;
u64 FactorialMinusOne = 1 * 2 * 3 * 4 * 5 - 1;
```

`;`

Does nothing.

## Expressions
An expression consists of atoms and operators (or just an atom on its own).

### Operator precedence
Operators are the following (each line is ordered from high to low precedence.
- unary prefixes `+ - ! ~` and grouping `()`
- bit shift operators `<< >>`
- multiplication `*`, integer division `//`, modulo `%`
- bitwise and `&`
- bitwise xor `^`
- bitwise or `|`
- add `+`, subtract `-`
- logical and `&&`
- logical or `||`

### Notes about specific operators
Bit shifts are modulo 64, so `a << 65` is the same as `a << 1`. If a number is shifted by a negative number, it is converted to u64, so `a << -1` is the same as `a << (1 << 64 - 1)` is the same as `a << 63`.

Integer division gives the quotient from truncated division (`16 // 3 == 5 == -16 // -3`, `16 // -3 == -5 == -16 // 3`), whereas the modulo operator uses the remainder from floored division (`16 % 3 == 1`, `16 % -3 == -2`, `-16 % 3 == 2`, `-16 % -3 == -1`). This is inconsistent but the floored version of modulo is more usable than the truncated version.

## Atoms
An atom is one of the following:
- A string literal beginning and ending with `"` characters. String literals can contain line breaks and escape characters (`\" \' \\ \n \t \v \f \r \b \0`, `\x??` where `??` is an ASCII hexadecimal code, and `\` followed by a line break which represents no character).
- A series of string literals (combined together into 1 string)
- An integer literal (default decimal, but also includes hexadecimal, octal and binary literals with the `0x`, `0o` and `0b` prefixes respectively). The suffix `u` can be added to a literal to make it u64. Integer literals with no prefix are of type i64 unless they represent a value larger than 2^63 in which case they are u64.
- An identifier (variable). `write <identifier>;` converts the identifier's data to a string and outputs it.

# Future
Much more is planned for Basque and all of the above will heavily change in the future.

