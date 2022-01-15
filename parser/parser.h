// See LICENSE for copyright/license information

#ifndef BA__PARSER_H
#define BA__PARSER_H

#include "op.h"
#include "func+var.h"
#include "exp.h"

// ----- Forward declarations -----
u8 ba_PStmt(struct ba_Controller* ctr);
// --------------------------------

u8 ba_PAccept(u64 type, struct ba_Controller* ctr) {
	if (!ctr->lex || (ctr->lex->type != type)) {
		(ctr->lex->type == BA_TK_FILECHANGE) && 
			(ctr->currPath = ctr->lex->val);
		return 0;
	}
	ctr->lex = ba_DelLexeme(ctr->lex);
	return 1;
}

u8 ba_PExpect(u64 type, struct ba_Controller* ctr) {
	if (!ba_PAccept(type, ctr)) {
		if (!ctr->lex->line) {
			fprintf(stderr, "Error: expected %s at end of file in %s\n", 
				ba_GetLexemeStr(type), ctr->currPath);
		}
		else {
			fprintf(stderr, "Error: expected %s at line %llu:%llu in %s\n",
				ba_GetLexemeStr(type), ctr->lex->line, ctr->lex->colStart,
				ctr->currPath);
		}
		exit(-1);
		return 0;
	}
	return 1;
}

/* base_type = ("u64" | "i64" | "void") { "*" } */
u8 ba_PBaseType(struct ba_Controller* ctr) {
	u64 lexType = ctr->lex->type;
	if (ba_PAccept(BA_TK_KW_U64, ctr) || ba_PAccept(BA_TK_KW_I64, ctr) ||
		ba_PAccept(BA_TK_KW_VOID, ctr)) 
	{
		struct ba_Type type = { BA_TYPE_TYPE, 0 };
		struct ba_Type* fundamentalType = malloc(sizeof(*fundamentalType));
		if (!fundamentalType) {
			return ba_ErrorMallocNoMem();
		}
		*fundamentalType = ba_GetTypeFromKeyword(lexType);
		while (ba_PAccept('*', ctr)) {
			struct ba_Type* newFundType = malloc(sizeof(*newFundType));
			newFundType->extraInfo = fundamentalType;
			newFundType->type = BA_TYPE_PTR;
			fundamentalType = newFundType;
		}
		type.extraInfo = fundamentalType;

		ba_PTkStkPush(ctr->pTkStk, /* val = */ 0, type, lexType, 
			/* isLValue = */ 0);
	}
	else {
		return 0;
	}

	return 1;
}

/* atom = lit_str { lit_str }
 *      | lit_int
 *      | identifier
 */
u8 ba_PAtom(struct ba_Controller* ctr) {
	u64 lexLine = ctr->lex->line;
	u64 lexColStart = ctr->lex->colStart;
	u64 lexValLen = ctr->lex->valLen;
	char* lexVal = 0;
	if (ctr->lex->val) {
		lexVal = malloc(lexValLen+1);
		if (!lexVal) {
			return ba_ErrorMallocNoMem();
		}
		strcpy(lexVal, ctr->lex->val);
	}

	// lit_str { lit_str }
	if (ba_PAccept(BA_TK_LITSTR, ctr)) {
		u64 len = lexValLen;
		char* str = malloc(len + 1);
		if (!str) {
			return ba_ErrorMallocNoMem();
		}
		strcpy(str, lexVal);
		
		// do-while prevents 1 more str literal from being consumed than needed
		do {
			if (ctr->lex->type != BA_TK_LITSTR) {
				break;
			}
			u64 oldLen = len;
			len += ctr->lex->valLen;
			
			if (len > BA_STACK_SIZE) {
				return ba_ExitMsg2(BA_EXIT_ERR, "string at", ctr->lex->line, 
					ctr->lex->colStart, ctr->currPath, 
					" too large to fit on the stack");
			}

			str = realloc(str, len+1);
			if (!str) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(str+oldLen, ctr->lex->val);
			str[len] = 0;
		}
		while (ba_PAccept(BA_TK_LITSTR, ctr));
		
		struct ba_Str* stkStr = malloc(sizeof(struct ba_Str));
		if (!stkStr) {
			return ba_ErrorMallocNoMem();
		}
		stkStr->str = str;
		stkStr->len = len;
		ba_PTkStkPush(ctr->pTkStk, (void*)stkStr, (struct ba_Type){0},
			BA_TK_LITSTR, /* isLValue = */ 0);
	}
	// lit_int
	else if (ba_PAccept(BA_TK_LITINT, ctr)) {
		u64 num;
		struct ba_Type type = { BA_TYPE_U64, 0 };
		if (lexVal[lexValLen-1] == 'u') {
			// ba_StrToU64 cannot parse the 'u' suffix so it must be removed
			lexVal[--lexValLen] = 0;
			num = ba_StrToU64(lexVal, lexLine, lexColStart, ctr->currPath);
		}
		else {
			num = ba_StrToU64(lexVal, lexLine, lexColStart, ctr->currPath);
			if (num < (1llu << 63)) {
				type = (struct ba_Type){ BA_TYPE_I64, 0 };
			}
		}
		ba_PTkStkPush(ctr->pTkStk, (void*)num, type, BA_TK_LITINT, 
			/* isLValue = */ 0);
	}
	// identifier
	else if (ba_PAccept(BA_TK_IDENTIFIER, ctr)) {
		struct ba_SymTable* stFoundIn = 0;
		struct ba_STVal* id = ba_STParentFind(ctr->currScope, &stFoundIn, lexVal);
		if (!id) {
			return ba_ErrorIdUndef(lexVal, lexLine, lexColStart, ctr->currPath);
		}
		ba_PTkStkPush(ctr->pTkStk, (void*)id, id->type, BA_TK_IDENTIFIER, 
			/* isLValue = */ 1);
	}
	// Other
	else {
		return 0;
	}

	free(lexVal);
	return 1;
}

// Auxiliary for ba_PStmt
void ba_PStmtWrite(struct ba_Controller* ctr, u64 len, char* str) {
	// Round up to nearest 0x10
	u64 memLen = len & 0xf;
	if (!len) {
		memLen = 0x10;
	}
	else if (memLen) {
		memLen ^= len;
		memLen += 0x10;
	}
	else {
		memLen = len;
	}
	
	// Allocate stack memory
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, memLen);

	u64 val = 0;
	u64 strIter = 0;
	u64 adrSub = memLen;
	
	// Store the string on the stack
	while (1) {
		if ((strIter && !(strIter & 7)) || (strIter >= len)) {
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, val);
			ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
				adrSub, BA_IM_RAX);
			
			if (!(strIter & 7)) {
				adrSub -= 8;
				val = 0;
			}
			if (strIter >= len) {
				break;
			}
		}
		val |= (u64)str[strIter] << ((strIter & 7) << 3);
		++strIter;
	}
	
	// write
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, len);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	
	// deallocate stack memory
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
}

// commaStmt = "," stmt
u8 ba_PCommaStmt(struct ba_Controller* ctr) {
	if (!ba_PAccept(',', ctr)) {
		return 0;
	}

	struct ba_SymTable* scope = ba_SymTableAddChild(ctr->currScope);
	ctr->currScope = scope;

	if (!ba_PStmt(ctr)) {
		return 0;
	}

	if (scope->dataSize) {
		ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, 
			BA_IM_IMM, scope->dataSize);
	}

	ctr->currScope = scope->parent;

	return 1;
}

// scope = "{" { stmt } "}"
u8 ba_PScope(struct ba_Controller* ctr) {
	if (!ba_PAccept('{', ctr)) {
		return 0;
	}

	struct ba_SymTable* scope = ba_SymTableAddChild(ctr->currScope);
	ctr->currScope = scope;

	while (ba_PStmt(ctr));

	if (scope->dataSize) {
		ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, 
			BA_IM_IMM, scope->dataSize);
	}

	ctr->currScope = scope->parent;

	return ba_PExpect('}', ctr);
}

/* stmt = "write" exp ";" 
 *      | "if" exp ( commaStmt | scope ) { "elif" exp ( commaStmt | scope ) }
 *        [ "else" ( commaStmt | scope ) ]
 *      | "while" exp ( commaStmt | scope )
 *      | "break" ";"
 *      | "return" [ exp ] ";"
 *      | "goto" identifier ";"
 *      | "include" lit_str { lit_str } ";"
 *      | scope
 *      | base_type identifier "(" [ { base_type [ identifier ] "," } 
 *        base_type [ identifier ] ] ")" ";"
 *      | base_type identifier "(" [ { base_type identifier [ "=" exp ] "," } 
 *        base_type identifier [ "=" exp ] ] ")" ( commaStmt | scope ) 
 *      | base_type identifier "=" exp ";"
 *      | identifier ":"
 *      | exp ";"
 *      | ";" 
 */
u8 ba_PStmt(struct ba_Controller* ctr) {
	u64 firstLine = ctr->lex->line;
	u64 firstCol = ctr->lex->colStart;

	// <file change>
	if (ctr->lex->type == BA_TK_FILECHANGE) {
		ctr->currPath = malloc(ctr->lex->valLen+1);
		strcpy(ctr->currPath, ctr->lex->val);
		return ba_PAccept(BA_TK_FILECHANGE, ctr);
	}
	// "write" ...
	else if (ba_PAccept(BA_TK_KW_WRITE, ctr)) {
		// TODO: this should eventually be replaced with a Write() function
		// and a lone string literal statement
	
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		// ... exp ...
		if (!ba_PExp(ctr)) {
			return 0;
		}

		char* str;
		u64 len;
		
		struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
		if (!stkItem) {
			return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col, 
				ctr->currPath);
		}

		if (stkItem->lexemeType == BA_TK_LITSTR) {
			str = ((struct ba_Str*)stkItem->val)->str;
			len = ((struct ba_Str*)stkItem->val)->len;
		}
		// Everything is printed as unsigned, this will be removed in the 
		// future anyway so i don't care about adding signed representation
		else if (ba_IsTypeIntegral(stkItem->typeInfo.type)) {
			if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
				str = ba_U64ToStr((u64)stkItem->val);
				len = strlen(str);
			}
			else {
				// U64ToStr is needed
				if (!ba_BltinFlagsTest(BA_BLTIN_U64ToStr)) {
					ba_BltinU64ToStr(ctr);
				}

				ba_POpMovArgToReg(ctr, stkItem, BA_IM_RAX, /* isLiteral = */ 0);
				if (stkItem->lexemeType == BA_TK_IMREGISTER && 
					(u64)stkItem->val != BA_IM_RAX) 
				{
					ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, (u64)stkItem->val);
				}
				// Note: stkItem->lexemeType won't ever be BA_TK_IMRBPSUB

				ba_AddIM(ctr, 2, BA_IM_LABELCALL, 
					ba_BltinLabels[BA_BLTIN_U64ToStr]);

				// write
				ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RAX);
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
				ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
				ba_AddIM(ctr, 1, BA_IM_SYSCALL);

				// deallocate stack memory
				ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 0x38);

				// ... ';'
				ba_PExpect(';', ctr);

				return 1;
			}
		}
		else {
			return ba_ExitMsg(BA_EXIT_ERR, "expression of incorrect type "
				"used with 'write' keyword on", line, col, ctr->currPath);
		}

		// ... ";"
		// Generates IM code
		if (ba_PExpect(';', ctr)) {
			ba_PStmtWrite(ctr, len, str);
			free(str);
			free(stkItem);
			return 1;
		}
	}
	// "if" ...
	else if (ba_PAccept(BA_TK_KW_IF, ctr)) {
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		bool hasReachedElse = 0;

		u64 endLblId = ctr->labelCnt++;

		while (ba_PExp(ctr)) {
			struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
			if (!stkItem) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col,
					ctr->currPath);
			}

			u64 lblId = ctr->labelCnt++;
			u64 reg = BA_IM_RAX;

			if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
				if (!ba_IsTypeNumeric(stkItem->typeInfo.type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "cannot use non-numeric literal "
						"as condition on", line, col, ctr->currPath);
				}

				if (stkItem->val) {
					ba_ExitMsg(BA_EXIT_WARN, "condition will always "
						"be true on", line, col, ctr->currPath);
				}
				else {
					ba_ExitMsg(BA_EXIT_WARN, "condition will always "
						"be false on", line, col, ctr->currPath);
					ba_AddIM(ctr, 2, BA_IM_LABELJMP, lblId);
				}
			}
			else {
				if (stkItem->lexemeType == BA_TK_IMREGISTER) {
					reg = (u64)stkItem->val;
				}
				else if (stkItem->lexemeType == BA_TK_IDENTIFIER) {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_ADRADD, BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, stkItem->val));
				}
				ba_AddIM(ctr, 3, BA_IM_TEST, reg, reg);
				ba_AddIM(ctr, 2, BA_IM_LABELJZ, lblId);
			}
		
			if (!ba_PCommaStmt(ctr) && !ba_PScope(ctr)) {
				return 0;
			}

			if (ctr->lex->type == BA_TK_KW_ELIF || ctr->lex->type == BA_TK_KW_ELSE) {
				ba_AddIM(ctr, 2, BA_IM_LABELJMP, endLblId);
			}
			
			ba_AddIM(ctr, 2, BA_IM_LABEL, lblId);
			
			if (ba_PAccept(BA_TK_KW_ELSE, ctr)) {
				hasReachedElse = 1;
				break;
			}
			else if (!ba_PAccept(BA_TK_KW_ELIF, ctr)) {
				break;
			}
		}

		if (hasReachedElse) {
			if (!ba_PCommaStmt(ctr) && !ba_PScope(ctr)) {
				return 0;
			}
		}

		// In some cases may be a duplicate label
		ba_AddIM(ctr, 2, BA_IM_LABEL, endLblId);

		return 1;
	}
	// "while" ...
	else if (ba_PAccept(BA_TK_KW_WHILE, ctr)) {
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		u64 startLblId = ctr->labelCnt++;
		ba_AddIM(ctr, 2, BA_IM_LABEL, startLblId);

		// ... exp ...
		if (!ba_PExp(ctr)) {
			return 0;
		}

		struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
		if (!stkItem) {
			return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col, 
				ctr->currPath);
		}

		u64 endLblId = ctr->labelCnt++;
		u64 reg = BA_IM_RAX;

		if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
			if (!ba_IsTypeNumeric(stkItem->typeInfo.type)) {
				return ba_ExitMsg(BA_EXIT_ERR, "cannot use non-numeric literal "
					"as while loop condition on", line, col, ctr->currPath);
			}

			if (!stkItem->val) {
				ba_ExitMsg(BA_EXIT_WARN, "while loop condition will always "
					"be false on", line, col, ctr->currPath);
				ba_AddIM(ctr, 2, BA_IM_LABELJMP, endLblId);
			}
		}
		else {
			if (stkItem->lexemeType == BA_TK_IMREGISTER) {
				reg = (u64)stkItem->val;
			}
			else if (stkItem->lexemeType == BA_TK_IDENTIFIER) {
				ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, 
					BA_IM_ADRADD, BA_IM_RSP, 
					ba_CalcSTValOffset(ctr->currScope, stkItem->val));
			}
			ba_AddIM(ctr, 3, BA_IM_TEST, reg, reg);
			ba_AddIM(ctr, 2, BA_IM_LABELJZ, endLblId);
		}
		
		// ... ( commaStmt | scope ) ...
		ba_StkPush(ctr->breakLblStk, (void*)endLblId);
		if (!ba_PCommaStmt(ctr) && !ba_PScope(ctr)) {
			return 0;
		}
		ba_StkPop(ctr->breakLblStk);

		ba_AddIM(ctr, 2, BA_IM_LABELJMP, startLblId);
		ba_AddIM(ctr, 2, BA_IM_LABEL, endLblId);

		return 1;
	}
	// "break" ";"
	else if (ba_PAccept(BA_TK_KW_BREAK, ctr)) {
		if (!ctr->breakLblStk->count) {
			return ba_ExitMsg(BA_EXIT_ERR, "keyword 'break' used outside of "
				"loop on", firstLine, firstCol, ctr->currPath);
		}
		ba_AddIM(ctr, 2, BA_IM_LABELJMP, (u64)ba_StkTop(ctr->breakLblStk));
		return ba_PExpect(';', ctr);
	}
	// "return" [ exp ] ";"
	else if (ba_PAccept(BA_TK_KW_RETURN, ctr)) {
		if (!ctr->currFunc) {
			return ba_ExitMsg(BA_EXIT_ERR, "keyword 'return' used outside of "
				"func on", firstLine, firstCol, ctr->currPath);
		}
		if (!ctr->lex) {
			ba_PExpect(';', ctr);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		if (ba_PExp(ctr)) {
			if (ctr->currFunc->retType.type == BA_TYPE_VOID) {
				return ba_ExitMsg(BA_EXIT_ERR, "returning value from func with "
					"return type 'void' on", line, col, ctr->currPath);
			}

			// Move return value into rax
			// TODO: or onto stack if too big
			
			struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
			if (!stkItem) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col,
					ctr->currPath);
			}
			if (stkItem->lexemeType == BA_TK_LITSTR) {
				return ba_ExitMsg(BA_EXIT_ERR, "returning string literal from "
					"a func currently not implemented,", line, col, ctr->currPath);
			}
			if (ba_IsTypeIntegral(stkItem->typeInfo.type)) {
				ba_POpMovArgToReg(ctr, stkItem, BA_IM_RAX, 
					ba_IsLexemeLiteral(stkItem->lexemeType));
				if (stkItem->lexemeType == BA_TK_IMREGISTER && 
					(u64)stkItem->val != BA_IM_RAX)
				{
					ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX,
						(u64)stkItem->val);
				}
				// Note: stkItem->lexemeType won't ever be BA_TK_IMRBPSUB
			}
		}
		else if (ctr->currFunc->retType.type != BA_TYPE_VOID) {
			return ba_ExitMsg(BA_EXIT_ERR, "returning without value from func "
				"that does not have return type 'void' on", line, col, 
				ctr->currPath);
		}

		if (ctr->currScope->dataSize && 
			ctr->currScope != ctr->currFunc->childScope) 
		{
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
		}

		ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->currFunc->lblEnd);
		ctr->currFunc->doesReturn = 1;
		return ba_PExpect(';', ctr);
	}
	// "goto" identifier ";"
	else if (ba_PAccept(BA_TK_KW_GOTO, ctr)) {
		u64 lblNameLen = ctr->lex->valLen;
		char* lblName = 0;
		if (ctr->lex->val) {
			lblName = malloc(lblNameLen+1);
			if (!lblName) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(lblName, ctr->lex->val);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		if (!ba_PExpect(BA_TK_IDENTIFIER, ctr)) {
			return 0;
		}

		u64 lblId = (u64)ba_HTGet(ctr->labelTable, lblName);

		if (lblId) {
			ba_AddIM(ctr, 2, BA_IM_LABELJMP, lblId);
		}
		else {
			ba_AddIM(ctr, 5, BA_IM_GOTO, (u64)lblName, line, col, 
				(u64)ctr->currPath);
		}
		
		return ba_PExpect(';', ctr);
	}
	// "include" lit_str { lit_str } ";"
	else if (ba_PAccept(BA_TK_KW_INCLUDE, ctr)) {
		u64 len = ctr->lex->valLen;
		char* fileName = malloc(len + 1);
		if (!fileName) {
			return ba_ErrorMallocNoMem();
		}
		strcpy(fileName, ctr->lex->val);
		ba_PExpect(BA_TK_LITSTR, ctr);

		// do-while prevents 1 more str literal from being consumed than needed
		do {
			if (ctr->lex->type != BA_TK_LITSTR) {
				break;
			}
			u64 oldLen = len;
			len += ctr->lex->valLen;
			
			if (len > BA_STACK_SIZE) {
				return ba_ExitMsg2(BA_EXIT_ERR, "string at", ctr->lex->line, 
					ctr->lex->colStart, ctr->currPath, 
					" too large to fit on the stack");
			}

			fileName = realloc(fileName, len+1);
			if (!fileName) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(fileName+oldLen, ctr->lex->val);
			fileName[len] = 0;
		}
		while (ba_PAccept(BA_TK_LITSTR, ctr));

		if (ctr->dir && fileName[0] != '/') {
			u64 dirLen = strlen(ctr->dir);
			char* relFileName = malloc(dirLen + len + 2);
			memcpy(relFileName, ctr->dir, dirLen+1);
			relFileName[dirLen] = '/';
			relFileName[dirLen+1] = 0;
			strcat(relFileName, fileName);
			free(fileName);
			fileName = relFileName;
		}

		FILE* includeFile = fopen(fileName, "r");
		if (!includeFile) {
			fprintf(stderr, "Error: cannot find file '%s' included on "
				"line %llu:%llu in %s\n", fileName, firstLine, firstCol, 
				ctr->currPath);
			exit(-1);
		}
		
		struct stat inclFileStat;
		fstat(fileno(includeFile), &inclFileStat);
		for (u64 i = 0; i < ctr->inclInodes->cnt; i++) {
			if (ctr->inclInodes->arr[i] == inclFileStat.st_ino) {
				return ba_PExpect(';', ctr);
			}
		}
		ba_PExpect(';', ctr);

		++ctr->inclInodes->cnt;
		(ctr->inclInodes->cnt > ctr->inclInodes->cap) && 
			ba_ResizeDynArr64(ctr->inclInodes);
		ctr->inclInodes->arr[ctr->inclInodes->cnt-1] = inclFileStat.st_ino;

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		struct ba_Lexeme* oldNextLex = ctr->lex;
		ctr->lex = ba_NewLexeme();
		ba_Tokenize(includeFile, ctr);
		struct ba_Lexeme* nextLex = ctr->lex;
		while (nextLex->next) {
			nextLex = nextLex->next;
		}
		nextLex->type = BA_TK_FILECHANGE;
		nextLex->line = line;
		nextLex->colStart = col;

		nextLex->valLen = strlen(ctr->currPath);
		nextLex->val = malloc(nextLex->valLen+1);
		strcpy(nextLex->val, ctr->currPath);

		nextLex->next = oldNextLex;
		ctr->currPath = fileName;
		return 1;
	}
	// scope
	else if (ba_PScope(ctr)) {
		return 1;
	}
	// base_type identifier ...
	else if (ba_PBaseType(ctr)) {
		struct ba_PTkStkItem* typeTk = ba_StkPop(ctr->pTkStk);
		struct ba_Type type = *(struct ba_Type*)(typeTk->typeInfo.extraInfo);

		if (!ctr->lex) {
			return 0;
		}
		
		u64 idNameLen = ctr->lex->valLen;
		char* idName = 0;
		if (ctr->lex->val) {
			idName = malloc(idNameLen+1);
			if (!idName) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(idName, ctr->lex->val);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		if (!ba_PExpect(BA_TK_IDENTIFIER, ctr)) {
			return 0;
		}

		// Function:
		// ... "(" ...
		if (ba_PAccept('(', ctr)) {
			return ba_PFuncDef(ctr, idName, line, col, type);
		}

		// Variable:
		// ... "=" exp ";"
		return ba_PVarDef(ctr, idName, line, col, type);
	}
	// identifier ":"
	else if (ctr->lex && ctr->lex->type == BA_TK_IDENTIFIER && 
		ctr->lex->next && ctr->lex->next->type == ':')
	{
		u64 lblNameLen = ctr->lex->valLen;
		char* lblName = 0;
		if (ctr->lex->val) {
			lblName = malloc(lblNameLen+1);
			if (!lblName) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(lblName, ctr->lex->val);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		ba_PExpect(BA_TK_IDENTIFIER, ctr);
		ba_PExpect(':', ctr);

		if (ba_HTGet(ctr->labelTable, lblName)) {
			return ba_ErrorVarRedef(lblName, line, col, ctr->currPath);
		}

		u64 lblId = ctr->labelCnt++;
		ba_AddIM(ctr, 2, BA_IM_LABEL, lblId);
		ba_HTSet(ctr->labelTable, lblName, (void*)lblId);

		return 1;
	}
	// exp ";"
	else if (ba_PExp(ctr)) {
		struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);

		// String literals by themselves in statements are written to standard output
		if (expItem->lexemeType == BA_TK_LITSTR) {
			char* str = ((struct ba_Str*)expItem->val)->str;
			ba_PStmtWrite(ctr, ((struct ba_Str*)expItem->val)->len, str);
			free(str);
			free(expItem);
		}

		if (!ba_PExpect(';', ctr)) {
			return 0;
		}

		return 1;
	}
	// ";"
	else if (ba_PAccept(';', ctr)) {
		ba_AddIM(ctr, 1, BA_IM_NOP);
		return 1;
	}

	return 0;
}

/* program = { stmt } eof */
u8 ba_Parse(struct ba_Controller* ctr) {
	while (!ba_PAccept(BA_TK_EOF, ctr)) {
		if (!ba_PStmt(ctr)) {
			// doesn't have anything to do with literals, 
			// this is just a decent buffer size
			char msg[BA_LITERAL_SIZE+1];
			if (ctr->lex->type == BA_TK_FILECHANGE) {
				sprintf(msg, "unexpected end of file %s, included on", 
					ctr->currPath);
				ctr->currPath = ctr->lex->val;
			}
			else {
				sprintf(msg, "unexpected %s at", 
					ba_GetLexemeStr(ctr->lex->type));
			}
			return ba_ExitMsg(BA_EXIT_ERR, msg, ctr->lex->line, 
				ctr->lex->colStart, ctr->currPath);
		}
	}
	
	// Exit
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 60);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);

	return 1;
}

#endif
