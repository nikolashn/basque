// See LICENSE for copyright/license information

#include "lexeme.h"
#include "exitmsg.h"

struct ba_Lexeme* ba_NewLexeme() {
	struct ba_Lexeme* lex = ba_CAlloc(1, sizeof(struct ba_Lexeme));
	return lex;
}

struct ba_Lexeme* ba_DelLexeme(struct ba_Lexeme* lex) {
	struct ba_Lexeme* next = lex->next;
	if (lex->type == BA_TK_LITSTR || lex->type == BA_TK_LITINT || 
		lex->type == BA_TK_IDENTIFIER)
	{
		free(lex->val);
	}
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
		lexType != BA_TK_IMSTACK && 
		lexType != BA_TK_IMREGISTER &&
		lexType != BA_TK_IMSTATIC;
}

struct ba_Type ba_GetTypeFromKeyword(u64 lexType) {
	switch (lexType) {
		case BA_TK_KW_U64:
			return (struct ba_Type){ BA_TYPE_U64, 0 };
		case BA_TK_KW_I64:
			return (struct ba_Type){ BA_TYPE_I64, 0 };
		case BA_TK_KW_U8:
			return (struct ba_Type){ BA_TYPE_U8, 0 };
		case BA_TK_KW_I8:
			return (struct ba_Type){ BA_TYPE_I8, 0 };
		case BA_TK_KW_BOOL:
			return (struct ba_Type){ BA_TYPE_BOOL, 0 };
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
		char* str = ba_MAlloc(4);
		str[0] = '\'';
		str[1] = lex;
		str[2] = '\'';
		str[3] = 0;
		return str;
	}
	switch (lex) {
		case BA_TK_GTE:         return "'>='";
		case BA_TK_RSHIFT:      return "'>>'";
		case BA_TK_LTE:         return "'<='";
		case BA_TK_LSHIFT:      return "'<<'";
		case BA_TK_IDIV:        return "'//'";
		case BA_TK_LOGAND:      return "'&&'";
		case BA_TK_LOGOR:       return "'||'";
		case BA_TK_DBEQUAL:     return "'=='";
		case BA_TK_NEQUAL:      return "'!='";
		case BA_TK_ADDEQ:       return "'+='";
		case BA_TK_SUBEQ:       return "'-='";
		case BA_TK_MULEQ:       return "'*='";
		case BA_TK_IDIVEQ:      return "'//='";
		case BA_TK_FDIVEQ:      return "'/='";
		case BA_TK_MODEQ:       return "'%='";
		case BA_TK_LSHIFTEQ:    return "'<<='";
		case BA_TK_RSHIFTEQ:    return "'>>='";
		case BA_TK_BITANDEQ:    return "'&='";
		case BA_TK_BITXOREQ:    return "'^='";
		case BA_TK_BITOREQ:     return "'|='";
		case BA_TK_INC:         return "'++'";
		case BA_TK_DEC:         return "'--'";
		case BA_TK_LITSTR:      return "string literal";
		case BA_TK_LITINT:      return "integer literal";
		case BA_TK_LITCHAR:     return "character literal";
		case BA_TK_IDENTIFIER:  return "identifier";
		case BA_TK_FSTRING:     return "formatted string";
		case BA_TK_KW_U64:      return "keyword 'u64'";
		case BA_TK_KW_I64:      return "keyword 'i64'";
		case BA_TK_KW_FWRITE:   return "keyword 'fwrite'";
		case BA_TK_KW_SWRITE:   return "keyword 'swrite'";
		case BA_TK_KW_IF:       return "keyword 'if'";
		case BA_TK_KW_ELIF:     return "keyword 'elif'";
		case BA_TK_KW_ELSE:     return "keyword 'else'";
		case BA_TK_KW_WHILE:    return "keyword 'while'";
		case BA_TK_KW_BREAK:    return "keyword 'break'";
		case BA_TK_KW_GOTO:     return "keyword 'goto'";
		case BA_TK_KW_RETURN:   return "keyword 'return'";
		case BA_TK_KW_VOID:     return "keyword 'void'";
		case BA_TK_KW_INCLUDE:  return "keyword 'include'";
		case BA_TK_KW_EXIT:     return "keyword 'exit'";
		case BA_TK_KW_U8:       return "keyword 'u8'";
		case BA_TK_KW_I8:       return "keyword 'i8'";
		case BA_TK_KW_GARBAGE:  return "keyword 'garbage'";
		case BA_TK_KW_LENGTHOF: return "keyword 'lengthof'";
	}
	return 0;
}

