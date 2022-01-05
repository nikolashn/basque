# The Basque Programming Language and C Basque Compiler Manual

Basque is extremely extremely alpha. The current features are very limited and rough around the edges. Nothing is stable; anything is subject to change.

## Program structure

Basque has no entry point function as there is in C. Code in the outer body of a program is executed as if it were in a main function.

## Types
### Integer types
The integer types in Basque are currently `i64` (64-bit signed integer), `u64` (64-bit unsigned integer) and `bool` (8-bit Boolean). Variables can be defined as `i64` or `u64` but currently not `bool`. Integer types are all commensurate with each other.

In the future the following integer types will also exist: `i32`, `u32`, `i16`, `u16`, `i8`, `u8`.

### String type
String (string literal) currently exists as a temporary type to represent string literals, but will eventually be removed once pointers are added.

### Functions
Funcs (functions) are a type that represents procedures. The return type of a func can be any type assignable to a variable, or `void` (representing no return value).

### Other future types
In the future there will be support for many other types: floating point numbers, pointers, structures, enumerations, and more.

## Syntax
### Comments
Single-line comments use a `#` symbol; multi-line comments begin with `#{` and end with `}#`, and do not nest.
```
# Single line comment
write "b a s q u e\n"; # Another single line comment
#{
Multiline
comment
}#
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
- type cast postfix `~ <type>`, func call `(,)`
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
Func calls are a func followed by a comma-seperated list of expressions (arguments) enclosed in parentheses, which may be empty. If the func has default parameters, then some arguments may be omitted. The following are some syntactically valid func calls: `foo()`, `bar(a, b)`, `baz(5 * SIZE, )`, `fleure(f(), , g(,))`

Only prefix increment and decrement are available in Basque. The operand of such operations must be an L-value.

The `$` operation evaluates to the size of its operand in bytes. Gives an error with the "string literal" type since it shouldn't really exist.

Bit shifts are modulo 64, so `a << 65` is the same as `a << 1`. If a number is shifted by a negative number, it is converted to u64, so `a << -1` is the same as `a << ((1 << 64) - 1)` is the same as `a << 63`.

Integer division gives the quotient from truncated division (`16 // 3 == 5 == -16 // -3`, `16 // -3 == -5 == -16 // 3`), whereas the modulo operator uses the remainder from floored division (`16 % 3 == 1`, `16 % -3 == -2`, `-16 % 3 == 2`, `-16 % -3 == -1`). This is inconsistent but the floored version of modulo is more usable than the truncated version.

The `&&` and `||` operators are short-circuiting and always result in type `bool`.

Comparison operators are non-associative: instead they work by chaining, like in mathematical notation. This means that `a <= b < c` is equivalent to `(a <= b) && (b < c)` (including short-circuiting), and not `(a <= b) < c` or `a <= (b < c)`. Comparison operators result in type `bool`.

All assignment operators are right-associative. The left-hand side of an assignment must be an L-value (an identifier or any expression that results in an identifier). So `a = 1`, `(msg) = "hi"` and `x = y = z` are valid, but `a + 1 = 1`, `"hi" = msg` and `(x = y) = z` are invalid.

### Statements
Statements are combinations of expressions that can be sequentially laid out to form a program. In this section, square brackets signify optionality, parentheses signify grouping, braces signify that a string can be repeated from 0 to infinite times, a bar signifies alternation, and words in angle brackets represent a group or class of expressions or tokens.

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

Normally, just evaluates the expression. If the expression is made up of string literals, the string literals are written to standard output as if it were a write statement.
```
5+2; # Does nothing
"yo\n"; # yo
```

#### Variable definition
Syntax: `<type> <identifier> = <expression>;`

Types currently are only `i64` and `u64`. An identifier begins with a letter and then may contain a series of letters, numbers and underscores.
```
u64 col = 0xfcf4d0u;
i64 _1234567 = 8;
u64 FactorialMinusOne = 1 * 2 * 3 * 4 * 5 - 1;
```

#### Func definition/forward declaration
Syntax: `<type> <identifier> (` [ { `<type>` [ `<identifier>` [ `= <expression>` ] ] `,` } `<type>` [ `<identifier>` [ `= <expression>` ] ] ] `)` ( `, <statement>` | `{` { `<statement>` } `}` | `;` )

Defines a func. The first token is the return type of the func, which may be `void`. A func can be forward declared if only a semicolon rather than statements are provided after the parameters list. For forward declarations, the identifier of a parameter may be omitted, but this is not the case for full definitions. Also, default arguments may be given in the definition of a function if it is not a forward declaration.

Examples of func definitions:
```
u64 Pow(u64 x, u64 y) {
	if y == 0u, return 1;
	if y == 1u, return x;
	if y & 1, return x * Pow(x * x, (y - 1) // 2);
	return Pow(x * x, y // 2);
}

i64 Add(i64 a = 0, i64 b = 0), return a + b;

void WriteI64(i64 num) {
	if num < 0 {
		write "-";
		num = -num;
	}
	write num;
}
```
Examples of forward declarations:
```
u64 Pow(u64, u64);
i64 Add(i64, i64 b);
void WriteI64(i64 num);
```

#### Return statement
Syntax: `return` [ `<expression>` ] `;`

Returns from a function, possibly with a return value. Can only be used in a function definition.

#### Include statement
Syntax: `include <string literal>` { `<string literal>` }

Includes a Basque source file at a given path. As there is no preprocessor in the C Basque compiler, include statements are resolved by the parser.

#### Semicolon
Syntax: `;`

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

Executes code while the expressed condition is true. Supports `break;` statements, but in Basque there is no `continue;`. Statements in a while loop are in a local scope.

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
Syntax (goto) `goto <identifier>;`

Syntax (label) `<identifier>:`

A goto statement jumps to a position in execution specified by a label.

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
For built in funcs, the C Basque compiler will pass the first argument for a call to RAX, then the rest on the stack. For user made funcs, all arguments are passed on the stack. RSP, RBP, and R8 - R15 must be preserved by funcs (currently not fully implemented as R8 - R15 are not yet used by user made funcs). Return values, like arguments, are stored first in RAX, then on the stack.

