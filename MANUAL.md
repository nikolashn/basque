# The Basque Programming Language and C Basque Compiler Manual

Basque is extremely extremely alpha. The current features are very limited and rough around the edges. Nothing is stable; anything is subject to change. This manual itself may be incomplete.

Note that throughout this manual, Extended Backus-Naur Form is used to specify syntax.

## Program structure

Basque has no entry point function as there is in C. Code in the outer body of a program is executed as if it were in a main function.

## Types
### Integer types
The integer types in Basque are currently `i64` (64-bit signed integer), `u64` (64-bit unsigned integer), `i8` (signed byte), `u8` (unsigned byte), `bool` (8-bit Boolean) and 64-bit pointers to any type (`i64*`, `u8[64]*`, `i64**`, etc.), as well as `void*` (generic pointer). Variables can be defined as any of those types. Integer types (including pointers) are all commensurate with each other.

In the future the following integer types will also exist: `i32`, `u32`, `i16`, `u16`.

### Arrays
Arrays in Basque are composed of a dimension on another type. This can be written as a rule `type[dimension]`. The type can be any valid data type, and each additional dimension with size *N* represents a repetition of a block of data of the fundamental type *N* times in series. Arrays in Basque are also different to those in C in some other ways, the main differences being:
- The syntax for declaring a variable as an array is different: Basque `i8[15] x;` vs C `signed char x[15];`, Basque `i64[6][7] arr;` vs C `long long arr[7][6];`, Basque `u8[99]* users;` vs C `unsigned char* users[99];`.
- Arrays cannot be casted to pointers in Basque. To get the address of an array, you will have to make a reference to it like any other variable.
- For the above reason, arrays are passed by value to functions and are value copied in assignments.
- Arrays can be casted and assigned to other array types of the same size, e.g. `u64[3]` can be casted to `i8[24]`.

### Structures
Structure types in Basque combine several pieces of data, which may be of different types, grouped into a single contiguous block of data. The syntax for declaring a structure type is to begin with the keyword `struct`, followed by an identifier (the struct name), a curly brace `{` and a list of member declarations (a type followed by an identifier and a semicolon), finally enclosed by a curly brace `}` and terminated with a semicolon `;`. When using the name of a struct as a type for a variable, unlike in C, the keyword `struct` is not used. For example, the following are all valid declarations of structure types:
```
struct String {
	u8* chars;
	u64 length;
};

struct DoublyLinkedList {
	i64 value;
	DoublyLinkedList* prev;
	DoublyLinkedList* next;
};

struct Person {
	String name;
	u64 age;
	DoublyLinkedList* items;
};
```

### Functions
Func (function) is a type that represents procedures. The return type of a func can be any type assignable to a variable, or `void` (representing no return value). Funcs may have optional parameters, similar to as in C++, and they may be defined in any scope.

A set of functions are available for use by default in a Basque program. These are called the core functions; more information about them is in a later section.

### Other future types
In the future there will be support for many other types: floating point numbers, enumerations, and more.

## Syntax
### Comments
Single-line comments use a `#` symbol; multi-line comments begin with `#{` and end with `#}`, and do not nest.
```
# Single line comment
"b a s q u e\n"; # Another single line comment
#{
Multiline
comment
#}
```
### Atoms
An atom is one of the following:
- A string literal beginning and ending with a `"` character. String literals can contain line breaks and escape sequences (`\" \' \\ \n \t \v \f \r \b \a \e \0`, `\x??` where `??` is an 8-bit hexadecimal code, and `\` followed by a line break which represents no character). String literals are zero-terminated `u8` arrays.
- A series of string literals (which then combine together into 1 string).
- An integer literal (default decimal, but also includes hexadecimal, octal and binary literals with the `0x`, `0o` and `0b` prefixes respectively). They may contain underscores after the first digit or prefix, which has no effect but can be used to make numbers more readable (Compare `3817193423` and `3_817_193_423`). The suffix `u` can be added to a literal to make it unsigned. Integer literals with no suffix are of type `i64` unless they represent a value larger than 2^63, in which case they are `u64`.
- A character literal beginning and ending with a `'` character. The character literal contains a representation of a single byte, either an ASCII character or an escape sequence.
- An identifier, which consists of an alphabetic character or underscore, followed by not more than 254 characters, each of which is either alphanumeric or an underscore.
- An array literal (see below).

#### Array literal
Syntax: `{ <expression>` { `, <expression>` } [ `,` ] `}`

For example, `{ 1 + 2 + 3, 4 + 5, 6 }`, `{ stop + 1, drop(), [roll], }`, `{ INIT_NUM }` are all syntactically valid array literals.

Array literals represent series of values combined into a block of data, to which arrays can be assigned to. They can only be used as the right hand side of an assignment or definition of a variable or default func parameter, as an argument of a func call, or as the expression of a return statement. They have no other operations (notably they cannot be casted). If the literal is smaller than the array it is being set to, then the remainder of the array will be filled with garbage.

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

Only prefix increment (`++`) and decrement (`--`) are available in Basque. The operand of such operations must be an L-value.

The `$` operation evaluates to the size of its operand in bytes. Its operand can be the name of a type rather than an expression. The evaluation for each type are as follows, where `T` represets any type, and `N` represents a particular arbitrary number:
- `bool`: 1
- `u8`, `i8`: 1
- `u64`, `i64`: 8
- `void*`, `T*`: 8
- `T[N]`: size of `T` multiplied by `N` (e.g. `u64[5]` has a size of 40)

The `&` unary prefix evaluates to the address of an l-value, string literal or array literal. For values of array type it results in the the address of the start of the array.

The `lengthof` operator evaluates to the amount of items in an array. `lengthof arr` is essentially syntactic sugar for `$arr // $[arr]`.

The dereferencing operator (or dereferencing list) `[,]` is a comma seperated list in square brackets. Basque `[a]`, `[a,b]`, `[a,b,c]`, etc. are equivalent to C `*a`, `a[b]`, `a[b][c]`, etc.

Bit shifts are modulo 64, so `a << 65` is the same as `a << 1`. If a number is shifted by a negative number, it is converted to u64, so `a << -1` is the same as `a << ((1 << 64) - 1)` is the same as `a << 63`.

Integer division gives the quotient from truncated division (`16 // 3 == 5 == -16 // -3`, `16 // -3 == -5 == -16 // 3`), whereas the modulo operator uses the remainder from floored division (`16 % 3 == 1`, `16 % -3 == -2`, `-16 % 3 == 2`, `-16 % -3 == -1`). This is inconsistent but the floored version of modulo is more usable than the truncated version.

Addition `+` and subtraction `-` do not have pointer arithmetic as in C. Arithmetical operations on pointers treat them as any other integer. Use `&[ptr,n]` instead of `ptr+n*$[ptr]` (which would be `ptr+n` with C pointer arithmetic).

The `&&` and `||` operators are short-circuiting everywhere and always result in type `bool`.

Comparison operators are non-associative: instead they work by chaining, like in mathematical notation or in Python. This means that `a <= b < c` is equivalent to `(a <= b) && (b < c)` (including short-circuiting), and not `(a <= b) < c` or `a <= (b < c)`. Comparison operators result in type `bool`.

All assignment operators are right-associative. The left-hand side of an assignment must be an L-value. So `a &= 1`, `(msg) = "hi"` and `x = y = z` are valid, but `a + 1 &= 1`, `"hi" = msg` and `(x = y) = z` are invalid.

### Statements
Statements are combinations of expressions that can be sequentially laid out to form a program.

#### Expression statements
Syntax: `<expression>;`

Normally, just evaluates the expression. If the expression is made up of string literals, the string literals are written to standard output.
```
5+2; # Does nothing
"yo\n"; # yo
```

#### Formatted string statements
Syntax: `<f-string>` { `<f-string>` } `;`

A formatted string (f-string) is a statement which allows expressions to be interpolated into a string for printing or writing to a file or buffer array. An f-string starts with `f` or `F` followed by `"`, then a series of characters, and terminated by another `"`. F-strings can also use the same escape sequences as string literals between the double quotes, and furthermore there are the following format specifiers and escape sequences, which are identified by starting with the character `%` and acting on expressions enclosed by braces `{` and `}`.

Format specifiers:

`%i{` expression `}`

Formats the expression as a signed 64-bit decimal integer (`i64`).

`%u{` expression `}`

Formats the expression as an unsigned 64-bit decimal integer (`u64`).

`%x{` expression `}`

Formats the expression as an unsigned 64-bit hexadecimal integer.

`%o{` expression `}`

Formats the expression as an unsigned 64-bit octal integer.

`%b{` expression `}`

Formats the expression as an unsigned 64-bit binary integer.

`%c{` expression `}`

Formats the expression as an ASCII character (`u8`).

`%s{` expression `}{` expression `}`

Formats the second expression a string, interpreting the first expression as the string's length, and the second as a `u8*` to the start of the string.

Escape sequences:

`%%` - character `%`
`%{` - character `{`
`%}` - character `}`

Example:
```
u8[] str = "hello!";
i64 num = -5;
u8 char = 'X';
u64 foo = 55;
f"%s{$str}{&str} there are %i{num} little monkeys, %c{char} marks the "
f"spot, both %u{45+foo}%% true facts\n";
```
Output of example:
```
hello! there are -5 little monkeys, X marks the spot, both 100% true facts
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

Defines a func. The first token is the return type of the func, which may be `void`, meaning that the func does not return a value. A func can be forward declared if only a semicolon rather than statements are provided after the parameters list. For forward declarations, the identifier of a parameter may be omitted, but this is not the case for full definitions. Also, default arguments may be given in the definition of a function if it is not a forward declaration.

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
		"-";
		num = -num;
	}
	f"%u{num}";
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

Includes a Basque source file at a given path, or a built-in include file (see section "Built-in include files"). As there is no preprocessor in the C Basque compiler, include statements are resolved by the parser.

Examples of valid include statements:
```
# local file
include "myfile.ba";
# built-in
include "sys";
```

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
	f"%u{1} is the loneliest number\n";
}
else {
	if x > 0, f"%u{x*(x-1)}";
	else {
		if x % 2 {
			f"%u{-x}";
		}
		else, f"%u{-x-1}";
	}
	"\n";
}
```

#### Assertions
Syntax: `assert <expression>;`

Will throw an error if the condition (the expression provided) is false. Non-numeric expressions are not permitted, as they cannot be converted to a boolean value.

```
i64 Pow(i64 a, i64 b) {
	assert b >= 0;
	i64 result = 1;
	while b; --b {
		result *= a;
	}
	return result;
}

Pow(5, 2);
Pow(-5, 2);
Pow(2, -5); # Exits with an assertion error
```

#### While loops
Syntax: `while <expression>` \[ `; <expression>` \] ( `"," <statement>` | `{` { `<statement>` } `}` )

Executes code while the condition expressed after the keyword `while` is true. If a semicolon `;` is also used, the expression after the semicolon will be executed at the end of each iteration of the loop. Supports `break;` statements, but in Basque there is no `continue;`. Statements in a while loop are in a local scope.

Examples:
```
u8 c = -1;
while c {
	Read(&c, 1);
	if 'a' <= c <= 'z' {
		c -= 0x20;
	}
	Write(&c, 1);
}
```
```
i64 i = 0;
while i < 10; ++i {
	if i != 0, ",";
	f"%i{i}";
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
	# c = 4; # Error
}
f"%u{a}\n";
# f"%u{b}\n"; # Error
# c = 25; # Error
```

#### Goto and label
Syntax (goto) `goto <identifier>;`

Syntax (label) `<identifier>:`

A goto statement jumps to a position in execution specified by a label. A goto statement can only jump to a label in the same scope as the statment itself, or to a label in an outer scope.

```
# Prints the even numbers between 0 and 15
u64 i = 0;
while 1 {
	if i == 15u, break;
	elif i % 2, goto lbl_LoopEnd;
	f"%u{i}\n";
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

## Core functions
The core functions of Basque are those functions which are included by default in the global name space.

### MemCopy
```
void MemCopy(void* dest, void* src, u64 size);
```
Copies `size` bytes from `src` to `dest`. The memory regions specified must not overlap.

### MemSet
```
void MemSet(void* ptr, u8 byte, u64 size);
```
Fills `size` bytes starting from the pointer `ptr` with the value `byte`.

## Built-in include files

### sys
```
include "sys";
```
The "sys" include contains funcs which serve as wrappers for Linux system calls. Be aware that the order of parameters of a wrapper func may be different to that of the system call which it is wrapping.

The system calls implemented so far are detailed below.

#### Read
```
i64 Read(void* buf, u64 count, i64 fd = 0);
```
System call number: 0

System call name: `read`

Notice that the order of parameters is different to that of the actual syscall (`fd` is at the end instead of the start).

#### Write
```
i64 Write(void* buf, u64 count, i64 fd = 1);
```
System call number: 1

System call name: `write`

Notice that the order of parameters is different to that of the actual syscall (`fd` is at the end instead of the start).

#### Open
```
i64 Open(u8* pathname, i64 flags, i64 mode = 0);
```
System call number: 2

System call name: `open`

#### Close
```
i64 Close(i64 fd);
```
System call number: 3

System call name: `close`

#### LSeek
```
i64 LSeek(i64 fd, i64 offset, i64 whence);
```
System call number: 8

System call name: `lseek`

#### MMap
```
void* MMap(void* addr = 0, u64 size, i64 prot, i64 flags, i64 fd = -1, i64 offset = 0);
```
System call number: 9

System call name: `mmap`

#### MProtect
```
i64 MProtect(void* addr, u64 size, i64 prot);
```
System call number: 10

System call name: `mprotect`

#### MUnmap
```
i64 MUnmap(void* addr, u64 size);
```
System call number: 11

System call name: `munmap`

#### Brk
```
void* Brk(void* addr = 0);
```
System call number: 12

System call name: `brk`

