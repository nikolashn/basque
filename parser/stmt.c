// See LICENSE for copyright/license information

#include "common.h"

/* stmt = "write" exp ";" 
 *      | "if" exp ( commaStmt | scope ) { "elif" exp ( commaStmt | scope ) }
 *        [ "else" ( commaStmt | scope ) ]
 *      | "while" exp [ "iter" exp ] ( commaStmt | scope )
 *      | "break" ";"
 *      | "return" [ exp ] ";"
 *      | "exit" exp ";"
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
u8 ba_PStmt(struct ba_Ctr* ctr) {
	u64 firstLine = ctr->lex->line;
	u64 firstCol = ctr->lex->colStart;

	// <file change>
	if (ctr->lex->type == BA_TK_FILECHANGE) {
		ctr->currPath = ba_MAlloc(ctr->lex->valLen+1);
		strcpy(ctr->currPath, ctr->lex->val);
		return ba_PAccept(BA_TK_FILECHANGE, ctr);
	}
	// "write" ...
	else if (ba_PAccept(BA_TK_KW_WRITE, ctr)) {
		/* TODO: this should eventually be replaced with a function
		 * and a lone string literal statement */
	
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
		else if (ba_IsTypeInt(stkItem->typeInfo)) {
			u64 size = ba_GetSizeOfType(stkItem->typeInfo);
			if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
				if (size < 8) {
					stkItem->val = (void*)((u64)stkItem->val & ((1llu<<(size*8))-1));
				}
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
				// Note: stkItem->lexemeType won't ever be BA_TK_IMSTACK
				
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);

				ba_AddIM(ctr, 2, BA_IM_LABELCALL, 
					ba_BltinLblGet(BA_BLTIN_U64ToStr));

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
				if (!ba_IsTypeNum(stkItem->typeInfo)) {
					return ba_ExitMsg(BA_EXIT_ERR, "cannot use non-numeric "
						"literal as condition on", line, col, ctr->currPath);
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
				u64 size = ba_GetSizeOfType(stkItem->typeInfo);
				if (stkItem->lexemeType == BA_TK_IMREGISTER) {
					reg = (u64)stkItem->val;
				}
				else if (stkItem->lexemeType == BA_TK_IDENTIFIER) {
					ba_POpMovIdToReg(ctr, stkItem->val, size, BA_IM_RAX, 
						/* isLea = */ 0);
				}
				u64 adjReg = ba_AdjRegSize(reg, size);
				ba_AddIM(ctr, 3, BA_IM_TEST, adjReg, adjReg);
				ba_AddIM(ctr, 2, BA_IM_LABELJZ, lblId);
			}
		
			if (!ba_PCommaStmt(ctr, 0) && !ba_PScope(ctr, 0)) {
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
			if (!ba_PCommaStmt(ctr, 0) && !ba_PScope(ctr, 0)) {
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
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
		ba_AddIM(ctr, 2, BA_IM_LABEL, startLblId);

		ctr->currScope = ba_SymTableAddChild(ctr->currScope);

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
			if (!ba_IsTypeNum(stkItem->typeInfo)) {
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
			u64 size = ba_GetSizeOfType(stkItem->typeInfo);
			if (stkItem->lexemeType == BA_TK_IMREGISTER) {
				reg = (u64)stkItem->val;
			}
			else if (stkItem->lexemeType == BA_TK_IDENTIFIER) {
				ba_POpMovIdToReg(ctr, stkItem->val, size, BA_IM_RAX, 
					/* isLea = */ 0);
			}
			u64 adjReg = ba_AdjRegSize(reg, 
				ba_GetSizeOfType(stkItem->typeInfo));
			ba_AddIM(ctr, 3, BA_IM_TEST, adjReg, adjReg);
			ba_AddIM(ctr, 2, BA_IM_LABELJZ, endLblId);
		}

		struct ba_IM* imIterBegin = 0;
		struct ba_IM* imIterEnd = 0;
		// ... "iter" ...
		if (ba_PAccept(BA_TK_KW_ITER, ctr)) {
			struct ba_IM* oldIM = ctr->im;
			imIterBegin = ba_NewIM();
			ctr->im = imIterBegin;

			// ... exp ...
			if (!ba_PExp(ctr)) {
				return 0;
			}
			stkItem = ba_StkPop(ctr->pTkStk);
			if (!stkItem) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col, 
					ctr->currPath);
			}
			
			imIterEnd = ctr->im;
			ctr->im = oldIM;
		}
		
		// ... ( commaStmt | scope ) ...
		ba_PBreakStkPush(ctr->pBreakStk, endLblId, ctr->currScope);
		if (!ba_PCommaStmt(ctr, ctr->currScope) && 
			!ba_PScope(ctr, ctr->currScope)) 
		{
			return 0;
		}
		ba_StkPop(ctr->pBreakStk);

		if (imIterBegin) {
			memcpy(ctr->im, imIterBegin, sizeof(*ctr->im));
			free(imIterBegin);
			ctr->im = imIterEnd;
		}

		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
		ba_AddIM(ctr, 2, BA_IM_LABELJMP, startLblId);

		ba_AddIM(ctr, 2, BA_IM_LABEL, endLblId);
		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);

		ctr->currScope = ctr->currScope->parent;

		return 1;
	}
	// "break" ";"
	else if (ba_PAccept(BA_TK_KW_BREAK, ctr)) {
		if (!ctr->pBreakStk->count) {
			return ba_ExitMsg(BA_EXIT_ERR, "keyword 'break' used outside of "
				"loop on", firstLine, firstCol, ctr->currPath);
		}
		struct ba_PLabel* label = ba_StkTop(ctr->pBreakStk);
		struct ba_SymTable* scope = ctr->currScope;
		while (scope != label->scope) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
			ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
			scope = scope->parent;
		}
		ba_AddIM(ctr, 2, BA_IM_LABELJMP, label->id);
		return ba_PExpect(';', ctr);
	}
	// "return" [ exp ] ";"
	else if (ba_PAccept(BA_TK_KW_RETURN, ctr)) {
		struct ba_Func* func = ctr->currScope->func;
		if (!func) {
			return ba_ExitMsg(BA_EXIT_ERR, "keyword 'return' used outside of "
				"func on", firstLine, firstCol, ctr->currPath);
		}
		if (!ctr->lex) {
			ba_PExpect(';', ctr);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		ba_StkPush(ctr->expCoercedTypeStk, &func->retType);
		ctr->isPermitArrLit = 1;
		if (ba_PExp(ctr)) {
			if (func->retType.type == BA_TYPE_VOID) {
				return ba_ExitMsg(BA_EXIT_ERR, "returning value from func with "
					"return type 'void' on", line, col, ctr->currPath);
			}

			struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
			if (!stkItem) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col,
					ctr->currPath);
			}
			if (!ba_IsTypeNum(func->retType) &&
				ba_IsTypeNum(stkItem->typeInfo)) 
			{
				return ba_ExitMsg(BA_EXIT_ERR, "returning numeric value "
					"from a function with non-numeric type", line, col, 
					ctr->currPath);
			}
			if (ba_IsTypeInt(stkItem->typeInfo)) {
				// Return value in rax
				bool isLiteral = ba_IsLexemeLiteral(stkItem->lexemeType);
				bool isConvToBool = stkItem->typeInfo.type != BA_TYPE_BOOL && 
					func->retType.type == BA_TYPE_BOOL;
				isConvToBool && isLiteral && 
					(stkItem->val = (void*)(bool)stkItem->val);
				ba_POpMovArgToReg(ctr, stkItem, BA_IM_RAX, isLiteral);
				
				if (stkItem->lexemeType == BA_TK_IMREGISTER && 
					(u64)stkItem->val != BA_IM_RAX)
				{ // Note: stkItem->lexemeType won't ever be BA_TK_IMSTACK
					ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, (u64)stkItem->val);
				}
				
				if (isConvToBool && !isLiteral) {
					ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RAX, BA_IM_RAX);
					ba_AddIM(ctr, 2, BA_IM_SETNZ, BA_IM_AL);
				}
			}
			else if (stkItem->typeInfo.type == BA_TYPE_ARR) {
				u64 retValSize = ba_GetSizeOfType(func->retType);
				ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RAX, BA_IM_ADRADD, BA_IM_RBP, 
					func->contextSize + func->paramStackSize - retValSize);
				struct ba_PTkStkItem* destItem = ba_MAlloc(sizeof(*destItem));
				destItem->lexemeType = 0;
				destItem->val = (void*)BA_IM_RAX;
				ba_PAssignArr(ctr, destItem, stkItem, retValSize);
			}
		}
		else if (func->retType.type != BA_TYPE_VOID) {
			return ba_ExitMsg(BA_EXIT_ERR, "returning without value from func "
				"that does not have return type 'void' on", line, col, 
				ctr->currPath);
		}
		ctr->isPermitArrLit = 0;
		ba_StkPop(ctr->expCoercedTypeStk);

		struct ba_SymTable* scope = ctr->currScope;
		while (scope != func->childScope) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
			ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
			scope = scope->parent;
		}
		ba_AddIM(ctr, 2, BA_IM_LABELJMP, func->lblEnd);
		func->doesReturn = 1;
		return ba_PExpect(';', ctr);
	}
	// "exit" exp ";"
	else if (ba_PAccept(BA_TK_KW_EXIT, ctr)) {
		if (!ba_PExp(ctr)) {
			return 0;
		}
		struct ba_PTkStkItem* exitCodeTk = ba_StkPop(ctr->pTkStk);
		if (!ba_IsTypeInt(exitCodeTk->typeInfo)) {
			return ba_ExitMsg(BA_EXIT_ERR, "exit code must be integral on",
				firstLine, firstCol, ctr->currPath);
		}
		ba_POpMovArgToReg(ctr, exitCodeTk, BA_IM_RDI, 
			ba_IsLexemeLiteral(exitCodeTk->lexemeType));
		if (exitCodeTk->lexemeType == BA_TK_IMREGISTER && 
			(u64)exitCodeTk->val == BA_IM_RDI) 
		{
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RDI, (u64)exitCodeTk->val);
		}
		ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 60);
		ba_AddIM(ctr, 1, BA_IM_SYSCALL);
		return ba_PExpect(';', ctr);
	}
	// "goto" identifier ";"
	else if (ba_PAccept(BA_TK_KW_GOTO, ctr)) {
		u64 lblNameLen = ctr->lex->valLen;
		char* lblName = 0;
		if (ctr->lex->val) {
			lblName = ba_MAlloc(lblNameLen+1);
			strcpy(lblName, ctr->lex->val);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		if (!ba_PExpect(BA_TK_IDENTIFIER, ctr)) {
			return 0;
		}

		struct ba_PLabel* label = ba_HTGet(ctr->labelTable, lblName);
		struct ba_SymTable* scope = ctr->currScope;

		if (label) {
			while (scope != label->scope) {
				ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
				ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
				scope = scope->parent;
			}
			(!scope) && ba_ErrorGoto(line, col, ctr->currPath);
			ba_AddIM(ctr, 2, BA_IM_LABELJMP, label->id);
		}
		else {
			ba_AddIM(ctr, 6, BA_IM_GOTO, (u64)lblName, (u64)ctr->currScope, 
				line, col, (u64)ctr->currPath);
		}
		
		return ba_PExpect(';', ctr);
	}
	// "include" lit_str { lit_str } ";"
	else if (ba_PAccept(BA_TK_KW_INCLUDE, ctr)) {
		u64 len = ctr->lex->valLen;
		char* fileName = ba_MAlloc(len + 1);
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

			fileName = ba_Realloc(fileName, len+1);
			strcpy(fileName+oldLen, ctr->lex->val);
			fileName[len] = 0;
		}
		while (ba_PAccept(BA_TK_LITSTR, ctr));

		// Built in includes
		if (!strcmp(fileName, "sys") && !ba_BltinFlagsTest(BA_BLTIN_Sys)) {
			ba_IncludeSys(ctr, firstLine, firstCol);
			return ba_PExpect(';', ctr);
		}
		else {
			if (ctr->dir && fileName[0] != '/') {
				u64 dirLen = strlen(ctr->dir);
				char* relFileName = ba_MAlloc(dirLen + len + 2);
				memcpy(relFileName, ctr->dir, dirLen+1);
				relFileName[dirLen] = '/';
				relFileName[dirLen+1] = 0;
				strcat(relFileName, fileName);
				free(fileName);
				fileName = relFileName;
			}

			// TODO: specifying paths to search as a command line flag
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
			nextLex->val = ba_MAlloc(nextLex->valLen+1);
			strcpy(nextLex->val, ctr->currPath);

			nextLex->next = oldNextLex;
			ctr->currPath = fileName;
			return 1;
		}
	}
	// scope
	else if (ba_PScope(ctr, 0)) {
		return 1;
	}
	// ( base_type | "void" ) identifier ...
	else if (ba_PBaseType(ctr, /* isInclVoid = */ 1, /* isInclIndefArr = */ 1)) {
		struct ba_PTkStkItem* typeTk = ba_StkPop(ctr->pTkStk);
		struct ba_Type type = *(struct ba_Type*)(typeTk->typeInfo.extraInfo);

		if (!ctr->lex) {
			return 0;
		}
		
		u64 idNameLen = ctr->lex->valLen;
		char* idName = 0;
		if (ctr->lex->val) {
			idName = ba_MAlloc(idNameLen+1);
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
			lblName = ba_MAlloc(lblNameLen+1);
			strcpy(lblName, ctr->lex->val);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		ba_PExpect(BA_TK_IDENTIFIER, ctr);
		ba_PExpect(':', ctr);

		if (ba_HTGet(ctr->labelTable, lblName)) {
			return ba_ErrorVarRedef(lblName, line, col, ctr->currPath);
		}

		struct ba_PLabel* label = malloc(sizeof(*label));
		label->id = ctr->labelCnt++;
		label->scope = ctr->currScope;
		ba_AddIM(ctr, 2, BA_IM_LABEL, label->id);
		ba_HTSet(ctr->labelTable, lblName, label);

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

u8 ba_PFuncDef(struct ba_Ctr* ctr, char* funcName, u64 line, u64 col, 
	struct ba_Type retType)
{
	struct ba_STVal* prevFuncIdVal = ba_HTGet(ctr->currScope->ht, funcName);
	if (prevFuncIdVal && (prevFuncIdVal->type.type != BA_TYPE_FUNC || 
		prevFuncIdVal->isInited))
	{
		return ba_ErrorVarRedef(funcName, line, col, ctr->currPath);
	}

	if (retType.type == BA_TYPE_ARR && 
		!((struct ba_ArrExtraInfo*)retType.extraInfo)->cnt) 
	{
		return ba_ExitMsg(BA_EXIT_ERR, "func return type cannot be indefinite "
			"size array on", line, col, ctr->currPath);
	}

	struct ba_STVal* funcIdVal = ba_MAlloc(sizeof(*funcIdVal));
	ba_HTSet(ctr->currScope->ht, funcName, (void*)funcIdVal);

	funcIdVal->scope = ctr->currScope;
	funcIdVal->type.type = BA_TYPE_FUNC;

	struct ba_Func* func = ba_NewFunc();
	funcIdVal->type.extraInfo = (void*)func;

	func->retType = retType;
	func->childScope = ba_SymTableAddChild(ctr->currScope);
	func->childScope->func = func;

	struct ba_FuncParam* param = ba_NewFuncParam();
	char* paramName = 0;
	func->firstParam = param;

	struct ba_IM* oldIM = ctr->im;
	ctr->im = func->imBegin;

	enum {
		TP_NONE = 0,
		TP_FWDDEC, // Forward declaration
		TP_FULLDEC
	}
	stmtType = 0;

	enum {
		ST_NONE = 0,
		ST_RPAREN,
		ST_PARAM,
		ST_COMMA,
		ST_DEFAULTVAL
	}
	state = 0;

	funcIdVal->isInited = 1;

	if (func->retType.type == BA_TYPE_ARR) {
		func->paramStackSize = ba_GetSizeOfType(func->retType);
	}

	while (state != ST_RPAREN) {
		switch (state) {
			case ST_RPAREN: return 0;

			case 0:
			{
				// ... ")" ...
				if (ba_PAccept(')', ctr)) {
					state = ST_RPAREN;
					break;
				}
				// Fallthrough into ST_COMMA
			}

			case ST_COMMA:
			{
				// ... base_type ...
				if (!ba_PBaseType(ctr, /* isInclVoid = */ 0, 
					/* isInclIndefArr = */ 0)) 
				{
					return 0;
				}
				
				++func->paramCnt;
				param->type = *(struct ba_Type*)((struct ba_PTkStkItem*)
					ba_StkPop(ctr->pTkStk))->typeInfo.extraInfo;

				if (ctr->lex->val) {
					paramName = ba_MAlloc(ctr->lex->valLen+1);
					strcpy(paramName, ctr->lex->val);
				}

				line = ctr->lex->line;
				col = ctr->lex->colStart;

				// ... identifier ...
				if (!ba_PAccept(BA_TK_IDENTIFIER, ctr)) {
					if (stmtType == TP_FULLDEC) {
						return 0;
					}
					stmtType = TP_FWDDEC;
					state = ST_PARAM;
					break;
				}

				if (ba_HTGet(func->childScope->ht, paramName)) {
					return ba_ErrorVarRedef(paramName, line, col, ctr->currPath);
				}

				param->stVal = ba_MAlloc(sizeof(struct ba_STVal));
				param->stVal->scope = func->childScope;
				param->stVal->type = param->type;

				u64 paramSize = ba_GetSizeOfType(param->stVal->type);
				func->paramStackSize += paramSize;
				param->stVal->address = func->paramStackSize;

				ba_HTSet(func->childScope->ht, paramName, (void*)param->stVal);

				state = ST_PARAM;
			}

			case ST_PARAM:
			{
				// ... ")" ...
				if (ba_PAccept(')', ctr)) {
					state = ST_RPAREN;
					break;
				}

				// ... "," ...
				if (ba_PAccept(',', ctr)) {
					state = ST_COMMA;
					goto BA_LBL_PFUNCDEF_ENDPARAM;
				}

				// ... "=" exp ...
				line = ctr->lex->line;
				col = ctr->lex->colStart;

				ba_StkPush(ctr->expCoercedTypeStk, &param->type);

				ctr->isPermitArrLit = 1;
				if (stmtType == TP_FWDDEC || !ba_PExpect('=', ctr) ||
					!ba_PExp(ctr))
				{
					return 0;
				}
				ctr->isPermitArrLit = 0;

				ba_StkPop(ctr->expCoercedTypeStk);
				
				struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);
				if (!expItem) {
					return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col,
						ctr->currPath);
				}
				
				if (!expItem->isConst) {
					return ba_ExitMsg(BA_EXIT_ERR, "default func parameter "
						"cannot be resolved at compile time on", line, col,
						ctr->currPath);
				}

				if ((ba_IsTypeNum(param->type) && 
					!ba_IsTypeNum(expItem->typeInfo)) ||
					(!ba_IsTypeNum(param->type) && 
					!ba_AreTypesEqual(param->type, expItem->typeInfo)))
				{
					return ba_ErrorConvertTypes(line, col, ctr->currPath,
						param->type, expItem->typeInfo);
				}

				param->defaultVal = (expItem->typeInfo.type == BA_TYPE_BOOL) 
					? (void*)(bool)expItem->val : expItem->val;
				param->hasDefaultVal = 1;
				state = ST_DEFAULTVAL;
				goto BA_LBL_PFUNCDEF_ENDPARAM;
			}

			case ST_DEFAULTVAL:
			{
				// ... ")" ...
				if (ba_PAccept(')', ctr)) {
					state = ST_RPAREN;
					break;
				}
				// ... "," ...
				if (!ba_PExpect(',', ctr)) {
					return 0;
				}
				state = ST_COMMA;
				goto BA_LBL_FUNCDEF_LOOPEND;
			}
		}

		goto BA_LBL_FUNCDEF_LOOPEND;

		BA_LBL_PFUNCDEF_ENDPARAM:
		param->next = ba_NewFuncParam();
		param = param->next;

		BA_LBL_FUNCDEF_LOOPEND:;
	}

	func->lblStart = ctr->labelCnt++;
	func->lblEnd = ctr->labelCnt++;

	ba_AddIM(ctr, 2, BA_IM_LABEL, func->lblStart);

	func->contextSize = 0x18; // return location + dynamic link + static link

	// TODO: praeserve registers
	
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP); // dynamic link
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, 8); // static link

	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP); // enter stack frame

	ctr->currScope = func->childScope;
	struct ba_Stk* oldBreakStk = ctr->pBreakStk;
	ctr->pBreakStk = ba_NewStk();

	if (!stmtType && ba_PAccept(';', ctr)) {
		funcIdVal->isInited = 0;
		stmtType = TP_FWDDEC;
	}
	else if (stmtType == TP_FWDDEC) {
		funcIdVal->isInited = 0;
		ba_PExpect(';', ctr);
	}
	else {
		// Correct param addresses
		param = func->firstParam;
		for (u64 i = 0; i < func->paramCnt; ++i) {
			param->stVal->address -= func->paramStackSize + func->contextSize;
			param = param->next;
		}

		if (ba_PAccept(',', ctr)) {
			stmtType = TP_FULLDEC;
			if (!ba_PStmt(ctr)) {
				return 0;
			}
		}
		else if (ba_PAccept('{', ctr)) {
			stmtType = TP_FULLDEC;
			while (ba_PStmt(ctr));
			ba_PExpect('}', ctr);
		}
		else {
			return 0;
		}
	}

	// Error for mismatch with forward declaration types
	if (prevFuncIdVal && 
		!ba_AreTypesEqual(prevFuncIdVal->type, funcIdVal->type)) 
	{
		fprintf(stderr, "Error: parameters of func %s declared on line "
			"%llu:%llu in %s incompatible with previously forward declared "
			"definition\n", funcName, line, col, ctr->currPath);
		exit(-1);
		free(prevFuncIdVal);
	}
	
	ba_AddIM(ctr, 2, BA_IM_LABEL, func->lblEnd);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP); // leave stack frame

	ba_DelStk(ctr->pBreakStk);
	ctr->pBreakStk = oldBreakStk;
	ctr->currScope = func->childScope->parent;
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // pop static pointer
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // pop dynamic pointer
	// TODO: restore registers
	ba_AddIM(ctr, 1, BA_IM_RET);

	func->imEnd = ctr->im;
	ctr->im = oldIM;

	if (stmtType == TP_FULLDEC && retType.type != BA_TYPE_VOID && 
		!func->doesReturn) 
	{
		fprintf(stderr, "Error: func '%s' (defined on line %llu:%llu in %s "
			"with return type %s) does not return a value\n", funcName, line, 
			col, ctr->currPath, ba_GetTypeStr(retType));
		exit(-1);
	}

	return 1;
}

u8 ba_PVarDef(struct ba_Ctr* ctr, char* idName, u64 line, u64 col, 
	struct ba_Type type)
{
	if (ba_HTGet(ctr->currScope->ht, idName)) {
		return ba_ErrorVarRedef(idName, line, col, ctr->currPath);
	}

	struct ba_SymTable* foundIn = 0;
	if (ba_STParentFind(ctr->currScope, &foundIn, idName)) {
		if (!ba_IsSilenceWarns()) {
			fprintf(stderr, "Warning: shadowing variable '%s' on "
				"line %llu:%llu in %s\n", idName, line, col, ctr->currPath);
		}
		if (ba_IsWarnsAsErrs()) {
			exit(-1);
		}
	}

	struct ba_STVal* idVal = ba_MAlloc(sizeof(struct ba_STVal));
	idVal->scope = ctr->currScope;
	idVal->type = type;

	if (idVal->type.type == BA_TYPE_VOID) {
		return ba_ErrorVarVoid(line, col, ctr->currPath);
	}
	
	ba_HTSet(ctr->currScope->ht, idName, (void*)idVal);

	line = ctr->lex->line;
	col = ctr->lex->colStart;

	ba_PExpect('=', ctr);

	bool isGarbage = 0;
	struct ba_PTkStkItem* expItem = 0;

	ba_StkPush(ctr->expCoercedTypeStk, &idVal->type);

	u64 dataSize = ba_GetSizeOfType(idVal->type);
	dataSize && (idVal->address = ctr->currScope->dataSize + dataSize);

	ctr->isPermitArrLit = 1;
	if (ba_PAccept(BA_TK_KW_GARBAGE, ctr)) {
		isGarbage = 1;
	}
	else if (ba_PExp(ctr)) {
		expItem = ba_StkPop(ctr->pTkStk);
		ba_POpAssignChecks(ctr, idVal->type, expItem, line, col);
	}
	else {
		return 0;
	}
	ctr->isPermitArrLit = 0;

	if (!dataSize && idVal->type.type == BA_TYPE_ARR) {
		dataSize = ba_GetSizeOfType(expItem->typeInfo);

		struct ba_ArrExtraInfo extraInfo = 
			*(struct ba_ArrExtraInfo*)expItem->typeInfo.extraInfo;
		((struct ba_ArrExtraInfo*)idVal->type.extraInfo)->cnt = 
			dataSize / ba_GetSizeOfType(extraInfo.type);
		
		idVal->address = ctr->currScope->dataSize + dataSize;
	}
	
	if (dataSize >= (1llu << 31)) {
		ba_ExitMsg(BA_EXIT_ERR, "data with size greater than 2147483647 on",
			line, col, ctr->currPath);
	}

	ba_StkPop(ctr->expCoercedTypeStk);

	ba_PExpect(';', ctr);
	
	if (ba_IsTypeNum(idVal->type)) {
		u64 reg = BA_IM_RAX;
		bool isExpLiteral = !isGarbage && ba_IsLexemeLiteral(expItem->lexemeType);
		
		if (!isGarbage) {
			if (expItem->lexemeType == BA_TK_IDENTIFIER) {
				bool isPopRbp = 0;
				i64 offset = ba_CalcVarOffset(ctr, expItem->val, &isPopRbp);
				ba_AddIM(ctr, 5, BA_IM_MOV, ba_AdjRegSize(BA_IM_RAX, dataSize), 
					offset < 0 ? BA_IM_ADRSUB : BA_IM_ADRADD, BA_IM_RBP,
					offset < 0 ? -offset : offset);
				if (isPopRbp) {
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
				}
			}
			if (isExpLiteral) {
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 
					(idVal->type.type == BA_TYPE_BOOL)
						? (bool)expItem->val : (u64)expItem->val);
			}
			(expItem->lexemeType == BA_TK_IMREGISTER) && 
				(reg = (u64)expItem->val);
		}

		ctr->currScope->dataSize += dataSize;

		if (dataSize == 8) {
			ba_AddIM(ctr, 2, BA_IM_PUSH, reg);
		}
		else if (dataSize == 1) {
			ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RSP);
		}
		else {
			ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, dataSize);
		}

		if (!isGarbage) {
			if (idVal->type.type == BA_TYPE_BOOL && !isExpLiteral) {
				ba_AddIM(ctr, 3, BA_IM_TEST, reg, reg);
				ba_AddIM(ctr, 2, BA_IM_SETNZ, reg - BA_IM_RAX + BA_IM_AL);
			}
			if (dataSize < 8) {
				ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RSP, 
					ba_AdjRegSize(reg, dataSize));
			}
		}
	}
	else if (idVal->type.type == BA_TYPE_ARR) {
		if (!dataSize) {
			ba_ExitMsg(BA_EXIT_ERR, "attempting to initialize array with " 
				"indefinite size to garbage on", line, col, ctr->currPath);
		}
		ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, dataSize);
		ctr->currScope->dataSize += dataSize;
		if (!isGarbage) {
			struct ba_PTkStkItem* destItem = ba_MAlloc(sizeof(*destItem));
			destItem->lexemeType = 0;
			destItem->val = (void*)BA_IM_RSP;
			ba_PAssignArr(ctr, destItem, expItem, dataSize);
		}
	}
	
	return 1;
}

void ba_PStmtWrite(struct ba_Ctr* ctr, u64 len, char* str) {
	// Round up to nearest 0x08
	u64 memLen = (len + 0x7) & ~0x7;
	if (!memLen) {
		memLen = 0x8;
	}
	
	// Allocate stack memory
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, memLen);

	u64 val = 0;
	u64 strIter = 0;
	u64 adrAdd = 0;
	
	// Store the string on the stack
	while (1) {
		if ((strIter && !(strIter & 7)) || (strIter >= len)) {
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, val);
			ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RSP, 
				adrAdd, BA_IM_RAX);
			
			if (!(strIter & 7)) {
				adrAdd += 8;
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
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, memLen);
}

// commaStmt = "," stmt
u8 ba_PCommaStmt(struct ba_Ctr* ctr, struct ba_SymTable* scope) {
	if (!ba_PAccept(',', ctr)) {
		return 0;
	}

	if (!scope) {
		ctr->currScope = ba_SymTableAddChild(ctr->currScope);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	}
	
	if (!ba_PStmt(ctr)) {
		return 0;
	}

	if (!scope) {
		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
		ctr->currScope = ctr->currScope->parent;
	}

	return 1;
}

// scope = "{" { stmt } "}"
u8 ba_PScope(struct ba_Ctr* ctr, struct ba_SymTable* scope) {
	if (!ba_PAccept('{', ctr)) {
		return 0;
	}

	if (!scope) {
		ctr->currScope = ba_SymTableAddChild(ctr->currScope);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	}

	while (ba_PStmt(ctr));
	
	if (!scope) {
		ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
		ctr->currScope = ctr->currScope->parent;
	}

	return ba_PExpect('}', ctr);
}

