# The Basque Programming Language and C Basque Compiler Manual

Basque is extremely extremely alpha. The current features are very limited and rough around the edges. Nothing is stable; anything is subject to change. This manual itself may be incomplete.

## Program structure

Basque has no entry point function as there is in C. Code in the outer body of a program is executed as if it were in a main function.

## Types
### Integer types
The integer types in Basque are currently `i64` (64-bit signed integer), `u64` (64-bit unsigned integer), `i8` (signed byte), `u8` (unsigned byte), `bool` (8-bit Boolean) and 64-bit pointers to any type (`i64*`, `u8[64]*`, `i64**`, etc.), as well as `void*` (generic pointer). Variables can be defined as any of those types, except currently `bool` or any pointer derived from it. Integer types (including pointers) are all commensurate with each other.

In the future the following integer types will also exist: `i32`, `u32`, `i16`, `u16`, properly functioning `bool`.

### Arrays
Arrays in Basque are composed of a dimension on another type. This can be written as a rule `type[dimension]`. The type can be any valid data type, and each additional dimension with size *N* represents a repetition of a block of data of the fundamental type *N* times in series. Arrays in Basque are also different to those in C in some other ways, the main differences being:
- The syntax for declaring a variable as an array is different: Basque `i8[15] x;` vs C `signed char x[15];`, Basque `i64[6][7] arr;` vs C `long long arr[7][6];`, Basque `u8[99]* users;` vs C `unsigned char* users[99];`.
- Arrays cannot be cast to pointers in Basque. To get the address of an array, you will have to make a reference to it like any other variable.
- For the above reason, arrays are passed by value to functions and are value copied in assignments.
- Arrays can be casted to other array types of the same size, e.g. `u64[3]` can be casted to `i8[24]`.

### Functions
Func (function) is a type that represents procedures. The return type of a func can be any type assignable to a variable, or `void` (representing no return value). Funcs may have optional parameters, similar to as in C++.

### Other future types
In the future there will be support for many other types: floating point numbers, structures, enumerations, and more.

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
- A string literal beginning and ending with a `"` character. String literals can contain line breaks and escape sequences (`\" \' \\ \n \t \v \f \r \b \0`, `\x??` where `??` is an ASCII hexadecimal code, and `\` followed by a line break which represents no character). String literals are null-terminated `u8` arrays.
- A series of string literals (which then combine together into 1 string).
- An integer literal (default decimal, but also includes hexadecimal, octal and binary literals with the `0x`, `0o` and `0b` prefixes respectively). They may contain underscores after the first digit or prefix, which has no effect but can be used to make numbers more readable (Compare `3817193423` and `3_817_193_423`). The suffix `u` can be added to a literal to make it unsigned. Integer literals with no suffix are of type `i64` unless they represent a value larger than 2^63, in which case they are `u64`.
- A character literal beginning and ending with a `'` character. The character literal consists of either a single character or an escape sequence.
- An identifier (variable). `write <identifier>;` converts the identifier's data to a string and outputs it.
- An array literal (see below).

#### Array literal
Syntax: `{ <expression>` { `, <expression>` } [ `,` ] `}`

For example, `{ 1 + 2 + 3, 4 + 5, 6 }`, `{ stop, drop, roll, }`, `{ INIT_NUM }` are all syntactically valid array literals.

Array literals represent series of values combined into a block of data, to which arrays (and in future, structs) can be assigned to. They can only be used as the right hand side of an assignment or definition of a variable or default func parameter, as an argument of a func call, or as the expression of a return statement. They have no other operations (notably they cannot be casted). If the literal is smaller than the array it is being set to, then the remainder of the array will be filled with garbage.

### Expressions
An expression consists of atoms and operators (or just an atom on its own).

#### Operator precedence
Basque's operators are the following (each line is ordered from high to low precedence). All binary operators are left-associative unless specified in the notes section.
- type cast postfix `~ <type>`, func call `(,)`
- unary prefixes `+ - ! ~ ++ -- $ & lengthof`, grouping `()`, dereferencing `[,]`
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

#### L-values
An L-value (assignable expression) in Basque is an identifier or dereferencing expression.

#### Notes about specific operators
Func calls are a func followed by a comma-seperated list of expressions (arguments) enclosed in parentheses, which may be empty. If the func has default parameters, then some arguments may be omitted. The following are some syntactically valid func calls: `foo()`, `bar(a, b)`, `baz(5 * SIZE, )`, `fleure(f(), , g(,))`

Only prefix increment (`++`) and decrement (`--`) are available in Basque. The operand of such operations must be an L-value. With pointers, the increment/decrement is by the size of the dereferenced pointer, instead of just 1.

The `$` operation evaluates to the size of its operand in bytes.

The `&` unary prefix evaluates to the address of an l-value, string literal or array literal. For values of array type it results in the the address of the start of the array.

The `lengthof` operator evaluates to the amount of items in an array. `lengthof arr` is essentially syntactic sugar for `$arr // $[arr]`.

The dereferencing operator (or dereferencing list) `[,]` is a comma seperated list in square brackets. Basque `[a]`, `[a,b]`, `[a,b,c]`, etc. are equivalent to C `*a`, `a[b]`, `a[b][c]`, etc.

Bit shifts are modulo 64, so `a << 65` is the same as `a << 1`. If a number is shifted by a negative number, it is converted to u64, so `a << -1` is the same as `a << ((1 << 64) - 1)` is the same as `a << 63`.

Integer division gives the quotient from truncated division (`16 // 3 == 5 == -16 // -3`, `16 // -3 == -5 == -16 // 3`), whereas the modulo operator uses the remainder from floored division (`16 % 3 == 1`, `16 % -3 == -2`, `-16 % 3 == 2`, `-16 % -3 == -1`). This is inconsistent but the floored version of modulo is more usable than the truncated version.

Addition `+` and subtraction `-` do not have pointer arithmetic as in C. Adding to a pointer will just cast it to an integer. Use `&[ptr,n]` instead of `ptr+n*$[ptr]` (which would be `ptr+n` with C pointer arithmetic).

The `&&` and `||` operators are short-circuiting and always result in type `bool`.

Comparison operators are non-associative: instead they work by chaining, like in mathematical notation. This means that `a <= b < c` is equivalent to `(a <= b) && (b < c)` (including short-circuiting), and not `(a <= b) < c` or `a <= (b < c)`. Comparison operators result in type `bool`.

All assignment operators are right-associative. The left-hand side of an assignment must be an L-value. So `a = 1`, `(msg) = "hi"` and `x = y = z` are valid, but `a + 1 = 1`, `"hi" = msg` and `(x = y) = z` are invalid.

### Statements
Statements are combinations of expressions that can be sequentially laid out to form a program. In this section, 
- Square brackets [] signify optionality, 
- Parentheses () signify grouping, 
- Braces {} signify that a string can be repeated from 0 to infinite times, 
- A bar | signifies alternation, 
- Words in angle brackets `<>` represent a group or class of expressions or tokens.

#### Write statements
Syntax: `write <expression>;`

Outputs an expression to standard output (converting numeric expressions to `u64` and then to decimal strings of numbers). These are temporary and will eventually be replaced with library functions.
```
write "Hello world!\n"; # Hello world!
write 7*12; # 84
write -1; # 18446744073709551615
```

#### Expression statements
Syntax: `<expression>;`

Normally, just evaluates the expression. If the expression is made up of string literals, the string literals are written to standard output.
```
5+2; # Does nothing
"yo\n"; # yo
```

#### Variable definition
Syntax: `<type> <identifier> =` ( `<expression>` | `garbage` ) `;`

An identifier begins with a letter and then may contain a series of letters, numbers and underscores.
```
u64* vals = &firstVal;
u64 col = 0xfcf4d0u;
i8 _1234567 = 8;
i64 FactorialMinusOne = 1 * 2 * 3 * 4 * 5 - 1;
```

A variable must be initialized using an expression. If no specific value should be set, then the keyword `garbage` can be used:
```
u64 mysteriousNumber = garbage;
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

#### Exit statement
Syntax: `exit` `<expression>` `;`

Exits from a program with an exit code.

#### Include statement
Syntax: `include <string literal>` { `<string literal>` } `;`

Includes a Basque source file at a given path. As there is no preprocessor in the C Basque compiler, include statements are resolved by the parser.

#### Semicolon
Syntax: `;`

Does nothing.

#### Conditional statements
Syntax:

`if <expression>` ( `"," <statement>` | `{` { `<statement>` } `}` )

{ `elif <expression>` ( `"," <statement>` | `{` { `<statement>` } `}` ) }

[ `else` ( `"," <statement>` | `{` { `<statement>` } `}` ) ]

The classic if/else-if/else conditional statement. Multiple statements can be executed upon a condition using a block (wrapping statements in braces), or a single statement can be executed using a comma. Such statements are in both cases in their own local scope.

Conditions are cast to bool meaning that zero is false and any other value is true.

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

For loops and do-while statements will also be added in the future.

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

### ELF program layout
Either 2 or 3 ELF segments (all LOAD) are generated: a header segment and a code segment, and a static segment for storing array literals and string literals (in the future also static variables). All variables are currently stored on the stack.

### Calling convention
All func arguments are passed on the stack. RBP, and R8 - R15 must be preserved by funcs (currently not fully implemented as R8 - R15 are not yet used by user made funcs). Return values, like arguments, are stored first in RAX, then on the stack.

