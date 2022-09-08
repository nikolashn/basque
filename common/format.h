// See LICENSE for copyright/license information

#ifndef BA__COMMON_FORMAT_H
#define BA__COMMON_FORMAT_H

#include "common.h"
#include "exitmsg.h"
#include "types.h"

struct ba_FStr { // Linked list
	void* val; /* If !formatType, then char*, otherwise ba_Lexeme* */
	u64 len; /* If formatType is BA_FTYPE_STR, then this is a ba_Lexeme*, a 
	            pointer to the expression calculating the string's length. */
	struct ba_FStr* next;
	struct ba_FStr* last;
	u64 formatType; // If 0 then val is a char*
};

// Format specifiers
enum /* u8 */ {
	BA_FTYPE_NONE = 0,
	BA_FTYPE_I64  = 0x1,
	BA_FTYPE_U64  = 0x2,
	BA_FTYPE_HEX  = 0x3,
	BA_FTYPE_OCT  = 0x4,
	BA_FTYPE_BIN  = 0x5,
	BA_FTYPE_CHAR = 0x6,
	BA_FTYPE_STR  = 0x7,
};

struct ba_Type ba_GetFStrType(u64 formatType);
u64 ba_StrToU64(char* str, u64 line, u64 col, char* path);
char* ba_U64ToStr(u64 num, u64* lenPtr);
char* ba_I64ToStr(i64 num, u64* lenPtr);
char* ba_HexToStr(u64 num, u64* lenPtr);
char* ba_OctToStr(u64 num, u64* lenPtr);
char* ba_BinToStr(u64 num, u64* lenPtr);

#endif
