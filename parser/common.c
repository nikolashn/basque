// See LICENSE for copyright/license information

#include "common.h"

u8 ba_PAccept(u64 type, struct ba_Ctr* ctr) {
	if (!ctr->lex || ctr->lex->type != type) {
		(ctr->lex->type == BA_TK_FILECHANGE) && (ctr->currPath = ctr->lex->val);
		return 0;
	}
	ctr->lex = ba_DelLexeme(ctr->lex);
	return 1;
}

u8 ba_PExpect(u64 type, struct ba_Ctr* ctr) {
	if (!ba_PAccept(type, ctr)) {
		if (!ctr->lex->line) {
			fprintf(stderr, "Error: expected %s at end of file in %s\n", 
				ba_GetLexemeStr(type), ctr->currPath);
		}
		else {
			fprintf(stderr, "Error: expected %s at line %llu:%llu in %s\n",
				ba_GetLexemeStr(type), ctr->lex->line, ctr->lex->col,
				ctr->currPath);
		}
		exit(1);
		return 0;
	}
	return 1;
}

struct ba_Lexeme* ba_PLookAhead(u64 k, u64 type, struct ba_Ctr* ctr) {
	struct ba_Lexeme* lex = ctr->lex;
	while (k) {
		if (!ctr->lex) {
			return 0;
		}
		lex = ctr->lex->next;
		--k;
	}
	if (!lex || lex->type != type) {
		return 0;
	}
	return lex;
}

u8 ParsePtrOrArray(struct ba_Ctr* ctr, struct ba_Type* type, 
	bool* ptrIsVoid, bool isInclIndefArr) 
{
	u64 lexType = ctr->lex->type;
	while (ba_PAccept('*', ctr) || ba_PAccept('[', ctr)) {
		if (lexType == '*') {
			struct ba_Type* oldInfo = type->extraInfo;
			type->extraInfo = ba_MAlloc(sizeof(struct ba_Type));
			*(struct ba_Type*)type->extraInfo = 
				(struct ba_Type){ BA_TYPE_PTR, oldInfo };
			*ptrIsVoid = 0;
			lexType = ctr->lex->type;
			goto BA_LBL_PARSEPTRORARRAY_END;
		}

		if (*ptrIsVoid) {
			break;
		}

		if (lexType == '[') {
			struct ba_ArrExtraInfo* extraInfo = ba_MAlloc(sizeof(*extraInfo));
			struct ba_Stk* oldOpStk = ctr->pOpStk;
			ctr->pOpStk = ba_NewStk();
			if (ba_PAccept(']', ctr)) {
				isInclIndefArr ||
					ba_ExitMsg(BA_EXIT_ERR, "invalid usage of indefinite size "
						"array on", ctr->lex->line, ctr->lex->col, 
						ctr->currPath);
				extraInfo->cnt = 0;
			}
			else if (ba_PExp(ctr)) {
				struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);
				if (!ba_IsTypeInt(expItem->typeInfo) || !expItem->isConst) {
					return ba_ExitMsg2(BA_EXIT_ERR, "invalid array size on", 
						ctr->lex->line, ctr->lex->col, ctr->currPath, 
						", must be a constant integer");
				}
				extraInfo->cnt = (u64)expItem->val;
				ba_PExpect(']', ctr);
			}
			else {
				return 0;
			}
			ba_DelStk(ctr->pOpStk);
			ctr->pOpStk = oldOpStk;

			extraInfo->type = *(struct ba_Type*)type->extraInfo;
			*(struct ba_Type*)type->extraInfo = 
				(struct ba_Type){ BA_TYPE_ARR, extraInfo };

			if (!extraInfo->cnt) {
				break;
			}
		}
		
		lexType = ctr->lex->type;
		BA_LBL_PARSEPTRORARRAY_END:;
	}
	return 1;
}

/* base_type = ( "u64" | "i64" | "u8" | "i8" | "bool" | "void" "*" ) 
 *             { "*" | "[" exp "]" } [ "[" "]" ]
 *           | "void" # if isInclVoid */
u8 ba_PBaseType(struct ba_Ctr* ctr, bool isInclVoid, bool isInclIndefArr) {
	u64 lexType = ctr->lex->type;
	bool isVoid = lexType == BA_TK_KW_VOID;
	
	if (!ba_PAccept(BA_TK_KW_U64, ctr) && !ba_PAccept(BA_TK_KW_I64, ctr) && 
		!ba_PAccept(BA_TK_KW_U8, ctr) && !ba_PAccept(BA_TK_KW_I8, ctr) &&
		!ba_PAccept(BA_TK_KW_BOOL, ctr) && !ba_PAccept(BA_TK_KW_VOID, ctr))
	{
		return 0;
	}
	
	struct ba_Type type = { BA_TYPE_TYPE, 0 };
	type.extraInfo = ba_MAlloc(sizeof(struct ba_Type));
	*(struct ba_Type*)type.extraInfo = ba_GetTypeFromKeyword(lexType);
	
	if (!ParsePtrOrArray(ctr, &type, &isVoid, isInclIndefArr)) {
		return 0;
	}

	if (isVoid && !isInclVoid) {
		return ba_ExitMsg(BA_EXIT_ERR, "invalid use of void type on", 
			ctr->lex->line, ctr->lex->col, ctr->currPath);
	}

	ba_PTkStkPush(ctr->pTkStk, /* val = */ 0, type, /* lexType = */ 0, 
		/* isLValue = */ 0, /* isConst = */ 1);
	return 1;
}

/* id_type = identifier { "*" | "[" exp "]" } [ "[" "]" ] */
u8 ba_PIdType(struct ba_Ctr* ctr, bool isInclIndefArr) {
	if (!ba_PLookAhead(0, BA_TK_IDENTIFIER, ctr)) {
		return 0;
	}

	struct ba_STVal* id = ba_STParentFind(ctr->currScope, 
		/* stFoundInPtr = */ 0, ctr->lex->val);
	if (!id || id->type.type != BA_TYPE_TYPE) {
		return 0;
	}
	ctr->lex = ba_DelLexeme(ctr->lex);

	struct ba_Type type = id->type;
	bool isVoid = 0; // Not used; the followin func just needs a real address

	if (!ParsePtrOrArray(ctr, &type, &isVoid, isInclIndefArr)) {
		return 0;
	}
	
	ba_PTkStkPush(ctr->pTkStk, /* val = */ 0, type, /* lexType = */ 0, 
		/* isLValue = */ 0, /* isConst = */ 1);
	return 1;
}

u8 ba_PPlainType(struct ba_Ctr* ctr, bool isInclVoid, bool isInclIndefArr) {
	u8 retVal = ba_PBaseType(ctr, isInclVoid, isInclIndefArr);
	if (retVal) {
		return retVal;
	}
	return ba_PIdType(ctr, isInclIndefArr);
}

