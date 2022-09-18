// See LICENSE for copyright/license information

#ifndef BA__COMMON_LEXEME_H
#define BA__COMMON_LEXEME_H

#include "types.h"

// Single-character lexemes not listed here use their ASCII values
enum {
	BA_TK_EOF         = 0,

	BA_TK_GTE         = 0x100,
	BA_TK_RSHIFT      = 0x101,
	BA_TK_LTE         = 0x102,
	BA_TK_LSHIFT      = 0x103,
	BA_TK_IDIV        = 0x104,
	BA_TK_LOGAND      = 0x105,
	BA_TK_LOGOR       = 0x106,
	BA_TK_DBEQUAL     = 0x107,
	BA_TK_NEQUAL      = 0x108,
	BA_TK_ADDEQ       = 0x109,
	BA_TK_SUBEQ       = 0x10a,
	BA_TK_MULEQ       = 0x10b,
	BA_TK_IDIVEQ      = 0x10c,
	BA_TK_FDIVEQ      = 0x10d,
	BA_TK_MODEQ       = 0x10e,
	BA_TK_LSHIFTEQ    = 0x10f,
	BA_TK_RSHIFTEQ    = 0x110,
	BA_TK_BITANDEQ    = 0x111,
	BA_TK_BITXOREQ    = 0x112,
	BA_TK_BITOREQ     = 0x113,
	BA_TK_INC         = 0x114,
	BA_TK_DEC         = 0x115,

	BA_TK_LITSTR      = 0x200,
	BA_TK_LITINT      = 0x201,
	BA_TK_LITCHAR     = 0x202,
	BA_TK_IDENTIFIER  = 0x203,
	BA_TK_FSTRING     = 0x204,

	BA_TK_KW_U64      = 0x300,
	BA_TK_KW_I64      = 0x301,
	BA_TK_KW_IF       = 0x302,
	BA_TK_KW_ELIF     = 0x303,
	BA_TK_KW_ELSE     = 0x304,
	BA_TK_KW_WHILE    = 0x305,
	BA_TK_KW_BREAK    = 0x306,
	BA_TK_KW_GOTO     = 0x307,
	BA_TK_KW_RETURN   = 0x30e,
	BA_TK_KW_VOID     = 0x318,
	BA_TK_KW_INCLUDE  = 0x319,
	BA_TK_KW_EXIT     = 0x31a,
	BA_TK_KW_U8       = 0x31b,
	BA_TK_KW_I8       = 0x31c,
	BA_TK_KW_GARBAGE  = 0x31d,
	BA_TK_KW_LENGTHOF = 0x30f,
	BA_TK_KW_ITER     = 0x310,
	BA_TK_KW_BOOL     = 0x311,
	BA_TK_KW_ASSERT   = 0x312,

	// Used to change ctr->currPath
	BA_TK_FILECHANGE = 0xfff,

	/* Not actual lexemes but instead used by 
	 * the parser for intermediate values */
	BA_TK_IMREGISTER = 0x1000, // always 8 bytes
	BA_TK_IMSTACK    = 0x1001, // always 8 bytes
	BA_TK_IMSTATIC   = 0x1002, // size varies
};

struct ba_Lexeme {
	u64 type;
	u64 line;
	u64 col;
	char* val;
	u64 valLen;
	struct ba_Lexeme* next;
};

struct ba_Lexeme* ba_NewLexeme();
struct ba_Lexeme* ba_DelLexeme(struct ba_Lexeme* lex);
bool ba_IsLexemeCompoundAssign(u64 lexType);
bool ba_IsLexemeCompare(u64 lexType);
bool ba_IsLexemeLiteral(u64 lexType);
struct ba_Type ba_GetTypeFromKeyword(u64 lexType);
char* ba_GetLexemeStr(u64 lex);

#endif
