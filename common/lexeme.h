// See LICENSE for copyright/license information

#include "types.h"

#ifndef BA__LEXEME_H
#define BA__LEXEME_H

// Single-character lexemes not listed here use their ASCII values
enum {
	BA_TK_EOF        = 0,

	BA_TK_GTE        = 0x100,
	BA_TK_RSHIFT     = 0x101,
	BA_TK_LTE        = 0x102,
	BA_TK_LSHIFT     = 0x103,
	BA_TK_IDIV       = 0x104,
	BA_TK_LOGAND     = 0x105,
	BA_TK_LOGOR      = 0x106,
	BA_TK_DBEQUAL    = 0x107,
	BA_TK_NEQUAL     = 0x108,
	BA_TK_ADDEQ      = 0x109,
	BA_TK_SUBEQ      = 0x10a,
	BA_TK_MULEQ      = 0x10b,
	BA_TK_IDIVEQ     = 0x10c,
	BA_TK_FDIVEQ     = 0x10d,
	BA_TK_MODEQ      = 0x10e,
	BA_TK_LSHIFTEQ   = 0x10f,
	BA_TK_RSHIFTEQ   = 0x110,
	BA_TK_BITANDEQ   = 0x111,
	BA_TK_BITXOREQ   = 0x112,
	BA_TK_BITOREQ    = 0x113,
	BA_TK_INC        = 0x114,
	BA_TK_DEC        = 0x115,

	BA_TK_LITSTR     = 0x200,
	BA_TK_LITINT     = 0x201,
	BA_TK_IDENTIFIER = 0x202,

	BA_TK_KW_WRITE   = 0x300,
	BA_TK_KW_U64     = 0x301,
	BA_TK_KW_I64     = 0x302,
	BA_TK_KW_IF      = 0x303,
	BA_TK_KW_ELIF    = 0x304,
	BA_TK_KW_ELSE    = 0x305,
	BA_TK_KW_WHILE   = 0x306,
	BA_TK_KW_BREAK   = 0x307,
	BA_TK_KW_GOTO    = 0x308,
	BA_TK_KW_RETURN  = 0x309,
	BA_TK_KW_VOID    = 0x30a,
	BA_TK_KW_INCLUDE = 0x30b,

	// Used to change ctr->currPath
	BA_TK_FILECHANGE = 0xfff,

	/* Not actual lexemes but instead used by 
	 * the parser for intermediate values */
	BA_TK_IMREGISTER = 0x1000,
	BA_TK_IMRBPSUB   = 0x1001,
};

struct ba_Lexeme {
	u64 type;
	u64 line, colStart;
	char* val;
	u64 valLen;
	struct ba_Lexeme* next;
};

struct ba_Lexeme* ba_NewLexeme() {
	struct ba_Lexeme* lex = malloc(sizeof(struct ba_Lexeme));
	if (!lex) {
		ba_ErrorMallocNoMem();
	}
	lex->type = BA_TK_EOF;
	lex->line = 0;
	lex->colStart = 0;
	lex->val = 0;
	lex->valLen = 0;
	lex->next = 0;
	return lex;
}

struct ba_Lexeme* ba_DelLexeme(struct ba_Lexeme* lex) {
	struct ba_Lexeme* next = lex->next;
	free(lex->val);
	free(lex);
	return next;
}

bool ba_IsLexemeCompoundAssign(u64 lexType) {
	return lexType == BA_TK_ADDEQ || lexType == BA_TK_SUBEQ || 
		lexType == BA_TK_MULEQ || lexType == BA_TK_IDIVEQ || 
		lexType == BA_TK_FDIVEQ || lexType == BA_TK_MODEQ || 
		lexType == BA_TK_LSHIFTEQ || lexType == BA_TK_RSHIFTEQ || 
		lexType == BA_TK_BITANDEQ || lexType == BA_TK_BITXOREQ || 
		lexType == BA_TK_BITOREQ;
}

bool ba_IsLexemeCompare(u64 lexType) {
	return lexType == '<' || lexType == '>' || lexType == BA_TK_LTE || 
		lexType == BA_TK_GTE || lexType == BA_TK_DBEQUAL || 
		lexType == BA_TK_NEQUAL;
}

bool ba_IsLexemeLiteral(u64 lexType) {
	return lexType != BA_TK_IDENTIFIER && 
		lexType != BA_TK_IMRBPSUB && 
		lexType != BA_TK_IMREGISTER;
}

struct ba_Type ba_GetTypeFromKeyword(u64 lexType) {
	switch (lexType) {
		case BA_TK_KW_U64:
			return (struct ba_Type){ BA_TYPE_U64, 0 };
		case BA_TK_KW_I64:
			return (struct ba_Type){ BA_TYPE_I64, 0 };
		case BA_TK_KW_VOID:
			return (struct ba_Type){ BA_TYPE_VOID, 0 };
	}
	return (struct ba_Type){0};
}

char* ba_GetLexemeStr(u64 lex) {
	if (lex == BA_TK_EOF) {
		return "eof";
	}
	else if (lex <= 0xff) {
		char* str = malloc(4);
		if (!str) {
			ba_ErrorMallocNoMem();
		}
		str[0] = '\'';
		str[1] = lex;
		str[2] = '\'';
		str[3] = 0;
		return str;
	}
	switch (lex) {
		case BA_TK_GTE:        return "'>='";
		case BA_TK_RSHIFT:     return "'>>'";
		case BA_TK_LTE:        return "'<='";
		case BA_TK_LSHIFT:     return "'<<'";
		case BA_TK_IDIV:       return "'//'";
		case BA_TK_LOGAND:     return "'&&'";
		case BA_TK_LOGOR:      return "'||'";
		case BA_TK_DBEQUAL:    return "'=='";
		case BA_TK_NEQUAL:     return "'!='";
		case BA_TK_ADDEQ:      return "'+='";
		case BA_TK_SUBEQ:      return "'-='";
		case BA_TK_MULEQ:      return "'*='";
		case BA_TK_IDIVEQ:     return "'//='";
		case BA_TK_FDIVEQ:     return "'/='";
		case BA_TK_MODEQ:      return "'%='";
		case BA_TK_LSHIFTEQ:   return "'<<='";
		case BA_TK_RSHIFTEQ:   return "'>>='";
		case BA_TK_BITANDEQ:   return "'&='";
		case BA_TK_BITXOREQ:   return "'^='";
		case BA_TK_BITOREQ:    return "'|='";
		case BA_TK_INC:        return "'++'";
		case BA_TK_DEC:        return "'--'";
		case BA_TK_LITSTR:     return "string literal";
		case BA_TK_LITINT:     return "integer literal";
		case BA_TK_IDENTIFIER: return "identifier";
		case BA_TK_KW_U64:     return "keyword 'u64'";
		case BA_TK_KW_I64:     return "keyword 'i64'";
		case BA_TK_KW_WRITE:   return "keyword 'write'";
		case BA_TK_KW_IF:      return "keyword 'if'";
		case BA_TK_KW_ELIF:    return "keyword 'elif'";
		case BA_TK_KW_ELSE:    return "keyword 'else'";
		case BA_TK_KW_WHILE:   return "keyword 'while'";
		case BA_TK_KW_BREAK:   return "keyword 'break'";
		case BA_TK_KW_GOTO:    return "keyword 'goto'";
		case BA_TK_KW_RETURN:  return "keyword 'return'";
		case BA_TK_KW_VOID:    return "keyword 'void'";
		case BA_TK_KW_INCLUDE: return "keyword 'include'";
	}
	return 0;
}

#endif
