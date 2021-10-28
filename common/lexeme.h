// See LICENSE for copyright/license information

#ifndef BA__LEXEME_H
#define BA__LEXEME_H

// Single-character lexemes not listed here use their ASCII values
enum {
	BA_TK_EOF = 0,

	BA_TK_GTE = 0x100,
	BA_TK_RSHIFT = 0x101,
	BA_TK_LTE = 0x102,
	BA_TK_LSHIFT = 0x103,
	BA_TK_DBSLASH = 0x104,
	BA_TK_DBAMPD = 0x105,
	BA_TK_DBBAR = 0x106,
	BA_TK_DBEQUAL = 0x107,
	BA_TK_NEQUAL = 0x108,
	
	BA_TK_LITSTR = 0x200,
	BA_TK_LITINT = 0x201,
	BA_TK_IDENTIFIER = 0x202,
	
	BA_TK_KW_WRITE = 0x300,
	BA_TK_KW_U64 = 0x301,
	BA_TK_KW_I64 = 0x302,

	BA_TK__COUNT = 0x400,

	// Not actual lexemes but instead used by the parser for 
	// intermediate values
	BA_TK_IMREGISTER = 0x1000,
	BA_TK_IMRBPSUB = 0x1001,
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
		case BA_TK_GTE:
			return "'>='";
		case BA_TK_RSHIFT:
			return "'>>'";
		case BA_TK_LTE:
			return "'<='";
		case BA_TK_LSHIFT:
			return "'<<'";
		case BA_TK_DBSLASH:
			return "'//'";
		case BA_TK_DBAMPD:
			return "'&&'";
		case BA_TK_DBBAR:
			return "'||'";
		case BA_TK_DBEQUAL:
			return "'=='";
		case BA_TK_NEQUAL:
			return "'!='";
		case BA_TK_LITSTR:
			return "string literal";
		case BA_TK_LITINT:
			return "integer literal";
		case BA_TK_IDENTIFIER:
			return "identifier";
		case BA_TK_KW_WRITE:
			return "keyword 'write'";
	}
	return 0;
}

#endif
