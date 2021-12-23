# The Basque Programming Language and C Basque Compiler Manual

Basque is extremely extremely alpha. The current features are very limited and rough around the edges. Nothing is stable; anything is subject to change.

## Program structure

Basque has no entry point function as there is in C. Code in the outer body of a program can be executed as if it were in a main function.

## Types
Currently only string (string literal), `i64` (64-bit signed integer), `u64` (64-bit unsigned integer), `u8` (unsigned byte) exist. The last one, `u8`, only exists as the result of some operations and currently variables cannot be defined as `u8`. The string type will eventually be removed once pointers are added, and more types will be added like `i8` (signed byte), `f32` (32-bit floating-point number), as well as pointers, structures, functions, etc.

## Syntax
### Comments
Single-line comments use a `#` symbol; multi-line comments begin and end with `##`, and do not nest.
```
# Single line comment
write "b a s q u e\n"; # Another single line comment
##
Multiline
comment
##
```
### Atoms
An atom is one of the following:
- A string literal beginning and ending with a `"` character. String literals can contain line breaks and escape characters (`\" \' \\ \n \t \v \f \r \b \0`, `\x??` where `??` is an ASCII hexadecimal code, and `\` followed by a line break which represents no character).
- A series of string literals (which then combine together into 1 string).
- An integer literal (default decimal, but also includes hexadecimal, octal and binary literals with the `0x`, `0o` and `0b` prefixes respectively). They may contain underscores after the first digit or prefix, which has no effect but can be used to make numbers more readable (Compare `3817193423` and `3_817_193_423`). The suffix `u` can be added to a literal to make it unsigned. Integer literals with no suffix are of type `i64` unless they represent a value larger than 2^63, in which case they are `u64`.
- An identifier (variable). `write <identifier>;` converts the identifier's data to a string and outputs it.

### Expressions
An expression consists of atoms and operators (or just an atom on its own).

#### Operator precedence
Basque's operators are the following (each line is ordered from high to low precedence). All binary operators are left-associative unless specified in the notes section.
- type cast postfix `~ <type>`
- unary prefixes `+ - ! ~ ++ -- $` and grouping `()`
- bit shift operators `<< >>`
- multiplication `*`, integer division `//`, modulo `%`
- bitwise and `&`
- bitwise xor `^`
- bitwise or `|`
- add `+`, subtract `-`
- comparison `< > <= >= == !=`
- logical and `&&`
- logical or `||`
- assignment `= += -= &= ^= |= *= //= %= <<= >>=`

#### Notes about specific operators
Only prefix increment and decrement are available in Basque. The operand of such operations must be an L-value.

The `$` operator is an operator that evaluates to the size of its operand in bytes. Gives an error with the "string literal" type since it shouldn't really exist.

Bit shifts are modulo 64, so `a << 65` is the same as `a << 1`. If a number is shifted by a negative number, it is converted to u64, so `a << -1` is the same as `a << ((1 << 64) - 1)` is the same as `a << 63`.

Integer division gives the quotient from truncated division (`16 // 3 == 5 == -16 // -3`, `16 // -3 == -5 == -16 // 3`), whereas the modulo operator uses the remainder from floored division (`16 % 3 == 1`, `16 % -3 == -2`, `-16 % 3 == 2`, `-16 % -3 == -1`). This is inconsistent but the floored version of modulo is more usable than the truncated version.

The `&&` and `||` operators are short-circuiting and always result in type `u8`.

Comparison operators are non-associative: instead they work by chaining, like in mathematical notation. This means that `a <= b < c` is equivalent to `(a <= b) && (b < c)` (including short-circuiting), and not `(a <= b) < c` or `a <= (b < c)`. Comparison operators result in type `u8`.

All assignment operators are right-associative. The left-hand side of an assignment must be an L-value (an identifier or any expression that results in an identifier). So `a = 1`, `(msg) = "hi"` and `x = y = z` are valid, but `a + 1 = 1`, `"hi" = msg` and `(x = y) = z` are invalid.
### Statements
Statements are combinations of expressions that together form a program. In this section, square brackets signify optionality, parentheses signify grouping, braces signify that a string can be repeated from 0 to infinite times, a bar signifies alternation, and words in angle brackets represent a group or class of expressions or tokens.

#### Write statements
Syntax: `write <expression>;`

Outputs an expression to standard output (converting numeric expressions to `u64` and then to decimal strings of numbers)
```
write "Hello world!\n"; # Hello world!
write 7*12; # 84
write -1; # 18446744073709551615
```

#### Expression statements
Syntax: `<expression>;`

Does nothing generally, unless the expression is made up of string literals, in which case it is like a write statement.
```
5+2; # Does nothing
"yo\n"; # yo
```

#### Variable definition
Syntax: `<type> <identifier>` [ `= <expression>` ] `;`

Types currently are only `i64` and `u64`. An identifier begins with a letter and then may contain a series of letters, numbers and underscores. Identifiers are always initialized to 0.
```
u64 col = 0xfcf4d0u;
i64 _1234567;
u64 FactorialMinusOne = 1 * 2 * 3 * 4 * 5 - 1;
```

#### Semicolon
`;`

Does nothing (compiles to NOP).

#### Conditional statements
Syntax:

`if <expression>` ( `"," <statement>` | `{` { `<statement>` } `}` )

{ `elif <expression>` ( `"," <statement>` | `{` { `<statement>` } `}` ) }

[ `else` ( `"," <statement>` | `{` { `<statement>` } `}` ) ]

The classic if/elif/else conditional statement. Multiple statements can be executed upon a condition using a block (wrapping statements in braces), or a single statement can be executed using a comma. Such statements are in both cases in their own local scope.

For anything with conditions in Basque, zero is false and any other numeric value is true.

```
if x == 0 {
	"zero\n";
}
elif x == 1 {
	write 1;
	" is the loneliest number\n";
}
else {
	if x > 0, write x*(x-1);
	else {
		if x % 2 {
			write -x;
		}
		else, write -x-1;
	}
	"\n";
}
```

#### While loops
Syntax: `while <expression>` ( `"," <statement>` | `{` { `<statement>` } `}` )

Executes code while the expressed condition is true. Supports `break;` statements, but in Basque there is no `continue;`. Statements in a while loop block are in a local scope.

```
u64 i = 0;
while i < 10u {
	if i != 0u, ",";
	write i;
	++i;
}
"\n";
```

#### Unnamed scopes
Syntax: `{` { `<statement>` } `}`

Multiple statements can be simply wrapped in braces, creating an unnamed local scope or block.

```
u64 a = 500;
{
	u64 b = 5;
	{
		u64 c = 2;
	}
	# write c; # Error
}
write a; "\n";
# write b; # Error
# c = 25; # Error
```

#### Goto and label
Syntax (goto) `goto <identifier> ;`

Syntax (label) `<identifier> :`

A goto statement jumps to a position in execution specified by a label. Labels cannot have the same identifier as variables in a program.

```
# Prints the even numbers between 0 and 15
u64 i = 0;
while 1 {
	if i == 15u, break;
	elif i % 2, goto lbl_LoopEnd;
	write i; "\n";
	lbl_LoopEnd:
	++i;
}
```

## The C Basque compiler
The C Basque compiler compiles to statically-linked Linux ELF64 executables, with no section headers or symbol table.

### Calling convention
The C Basque compiler will pass the first argument for a call to RAX, then the rest on the stack. RBX is used to store the return location of a call. RSP, RBP, and R8 - R15 must be preserved by functions. Return values, like arguments, are stored first in RAX, then on the stack.

