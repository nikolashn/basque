// See LICENSE for copyright/license information

#ifndef BA__PARSER_H
#define BA__PARSER_H

#include "op.h"
#include "funcvar.h"

// ----- Forward declarations -----
u8 ba_PStmt(struct ba_Controller* ctr);
// --------------------------------

u8 ba_PAccept(u64 type, struct ba_Controller* ctr) {
	if (!ctr->lex || (ctr->lex->type != type)) {
		return 0;
	}
	ctr->lex = ba_DelLexeme(ctr->lex);
	return 1;
}

u8 ba_PExpect(u64 type, struct ba_Controller* ctr) {
	if (!ba_PAccept(type, ctr)) {
		if (!ctr->lex->line) {
			fprintf(stderr, "Error: expected %s at end of file\n", 
				ba_GetLexemeStr(type));
		}
		else {
			fprintf(stderr, "Error: expected %s at line %llu:%llu\n",
				ba_GetLexemeStr(type), ctr->lex->line, ctr->lex->colStart);
		}
		exit(-1);
		return 0;
	}
	return 1;
}

/* base_type = "u64" | "i64" | "void" */
u8 ba_PBaseType(struct ba_Controller* ctr) {
	u64 lexType = ctr->lex->type;
	if (ba_PAccept(BA_TK_KW_U64, ctr) || ba_PAccept(BA_TK_KW_I64, ctr) ||
		ba_PAccept(BA_TK_KW_VOID, ctr)) 
	{
		ba_PTkStkPush(ctr->pTkStk, /* val = */ 0, BA_TYPE_TYPE, lexType, 
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
					ctr->lex->colStart, " too large to fit on the stack");
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
		ba_PTkStkPush(ctr->pTkStk, (void*)stkStr, BA_TYPE_NONE, BA_TK_LITSTR, 
			/* isLValue = */ 0);
	}
	// lit_int
	else if (ba_PAccept(BA_TK_LITINT, ctr)) {
		u64 num;
		u64 type = BA_TYPE_U64;
		if (lexVal[lexValLen-1] == 'u') {
			// ba_StrToU64 cannot parse the 'u' suffix so it must be removed
			lexVal[--lexValLen] = 0;
			num = ba_StrToU64(lexVal, lexLine, lexColStart);
		}
		else {
			num = ba_StrToU64(lexVal, lexLine, lexColStart);
			if (num < (1llu << 63)) {
				type = BA_TYPE_I64;
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
			return ba_ErrorIdUndef(lexVal, lexLine, lexColStart);
		}
		if (!id->isInited) {
			ba_ExitMsg(BA_EXIT_WARN, "using uninitialized identifier on", 
				lexLine, lexColStart);
		}
		ba_PTkStkPush(ctr->pTkStk, (void*)id, id->type, 
			id->scope == ctr->globalST ? BA_TK_GLOBALID : BA_TK_LOCALID, 
			/* isLValue = */ 1);
	}
	// Other
	else {
		return 0;
	}

	free(lexVal);
	return 1;
}

// Any type of expression
u8 ba_PExp(struct ba_Controller* ctr) {
	// Reset comparison chain stacks
	ctr->cmpLblStk->count = 0;
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;

	// Parse as if following an operator or parse as if following an atom?
	bool isAfterAtom = 0;

	// Counts grouping parentheses to make sure they are balanced
	i64 paren = 0;
	
	while (1) {
		u64 lexType = ctr->lex->type;
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		// Start of an expression or after a binary operator
		if (!isAfterAtom) {
			// Prefix operator
			if (ba_PAccept('+', ctr) || ba_PAccept('-', ctr) || 
				ba_PAccept('!', ctr) || ba_PAccept('~', ctr) || 
				ba_PAccept('(', ctr) || ba_PAccept('$', ctr) || 
				ba_PAccept(BA_TK_INC, ctr) || ba_PAccept(BA_TK_DEC, ctr))
			{
				// Left grouping parenthesis
				if (lexType == '(') {
					++paren;
					// Entering a new expression frame (reset whether is cmp chain)
					ba_StkPush(ctr->cmpRegStk, (void*)0);
				}
				ba_POpStkPush(ctr->pOpStk, line, col, lexType, BA_OP_PREFIX);
			}
			// Atom: note that ba_PAtom pushes the atom to pTkStk
			else if (ba_PAtom(ctr)) {
				isAfterAtom = 1;
			}
			else {
				return 0;
			}
		}
		// After an atom, postfix or grouped expression
		else {
			struct ba_POpStkItem* op = malloc(sizeof(*op));
			if (!op) {
				return ba_ErrorMallocNoMem();
			}

			op->line = line;
			op->col = col;
			op->lexemeType = lexType;
			op->syntax = 0;

			u64 nextLexType = ctr->lex->type;

			// Set syntax type
			if (ctr->lex->type == ')') {
				if (paren <= 0) {
					goto BA_LBL_PEXP_END;
				}
				ba_PAccept(')', ctr);
				op->syntax = BA_OP_POSTFIX;
			}
			else if (ba_PAccept('~', ctr)) {
				op->syntax = BA_OP_POSTFIX;

				if (!ba_PBaseType(ctr)) {
					return ba_ExitMsg(BA_EXIT_ERR, "cast to expression "
						"that is not a type on", op->line, op->col);
				}
			}
			else if (ba_PAccept(BA_TK_LSHIFT, ctr) || 
				ba_PAccept(BA_TK_RSHIFT, ctr) || ba_PAccept('*', ctr) || 
				ba_PAccept(BA_TK_IDIV, ctr) || ba_PAccept('%', ctr) || 
				ba_PAccept('&', ctr) || ba_PAccept('^', ctr) || 
				ba_PAccept('|', ctr) || ba_PAccept('+', ctr) || 
				ba_PAccept('-', ctr) || ba_PAccept(BA_TK_LOGAND, ctr) || 
				ba_PAccept(BA_TK_LOGOR, ctr) || ba_PAccept('=', ctr) || 
				(ba_IsLexemeCompoundAssign(nextLexType) && 
					ba_PAccept(nextLexType, ctr)) || 
				(ba_IsLexemeCompare(nextLexType) && 
					ba_PAccept(nextLexType, ctr)))
			{
				op->syntax = BA_OP_INFIX;
			}
			else {
				goto BA_LBL_PEXP_END;
			}

			// Right grouping parenthesis
			(lexType == ')') && --paren;

			if (ctr->pOpStk->count) {
				do {
					struct ba_POpStkItem* stkTop = ba_StkTop(ctr->pOpStk);
					u8 stkTopPrec = ba_POpPrecedence(stkTop);
					u8 opPrec = ba_POpPrecedence(op);
					bool willHandle = ba_POpIsRightAssoc(op)
						? stkTopPrec < opPrec : stkTopPrec <= opPrec;

					if (willHandle) {
						u8 handleResult = ba_POpHandle(ctr, op);
						if (!handleResult) {
							return 0;
						}
						// Left grouping parenthesis
						else if (handleResult == 2) {
							// Return to previous expression frame
							ba_StkPop(ctr->cmpRegStk);
							goto BA_LBL_PEXP_LOOPEND;
						}
					}
					else {
						break;
					}
				}
				while (ctr->pOpStk->count);
			}

			// Short circuiting: jmp ahead if right-hand side operand is unnecessary
			if (lexType == BA_TK_LOGAND || lexType == BA_TK_LOGOR) {
				struct ba_PTkStkItem* lhs = ba_StkPop(ctr->pTkStk);
				ba_StkPush(ctr->shortCircLblStk, (void*)ctr->labelCnt);

				u64 reg = (u64)lhs->val; // Kept only if lhs is a register
				
				if (lhs->lexemeType != BA_TK_IMREGISTER) {
					reg = ba_NextIMRegister(ctr);
				}

				if (!reg) { // Preserve rax
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
				}
				
				u64 realReg = reg ? reg : BA_IM_RAX;
				if (lhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realReg, BA_IM_DATASGMT,
						((struct ba_STVal*)lhs->val)->address);
				}
				else if (lhs->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, realReg, 
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, lhs->val));
				}
				else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, realReg, BA_IM_ADRSUB,
						BA_IM_RBP, (u64)lhs->val);
				}
				else if (lhs->lexemeType != BA_TK_IMREGISTER) {
					// Literal
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realReg, BA_IM_IMM, 
						(u64)lhs->val);
				}

				ba_AddIM(&ctr->im, 3, BA_IM_TEST, realReg, realReg);
				if (!reg) {
					ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
				}
				ba_AddIM(&ctr->im, 2, 
					lexType == BA_TK_LOGAND ? BA_IM_LABELJZ : BA_IM_LABELJNZ,
						ctr->labelCnt);

				lhs->lexemeType = BA_TK_IMREGISTER;
				lhs->val = (void*)reg;
				ba_StkPush(ctr->pTkStk, lhs);

				++ctr->labelCnt;
			}

			ba_StkPush(ctr->pOpStk, op);

			if (op->syntax == BA_OP_INFIX) {
				isAfterAtom = 0;
			}
		}

		BA_LBL_PEXP_LOOPEND:;
		
		if (paren < 0) {
			return ba_ExitMsg(BA_EXIT_ERR, "unmatched ')' on", line, col);
		}

		line = ctr->lex->line;
		col = ctr->lex->colStart;
	}

	BA_LBL_PEXP_END:;
	
	if (paren) {
		return 0;
	}
	
	while (ctr->pOpStk->count) {
		if (!ba_POpHandle(ctr, 0)) {
			return 0;
		}
	}

	ctr->usedRegisters = BA_IM_RAX;
	if (ctr->imStackCnt) {
		ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, ctr->imStackSize);
	}
	ctr->imStackCnt = 0;
	ctr->imStackSize = 0;

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
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(&ctr->im, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, memLen);

	u64 val = 0;
	u64 strIter = 0;
	u64 adrSub = memLen;
	
	// Store the string on the stack
	while (1) {
		if ((strIter && !(strIter & 7)) || (strIter >= len)) {
			ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, val);
			ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
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
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, len);
	ba_AddIM(&ctr->im, 1, BA_IM_SYSCALL);
	
	// deallocate stack memory
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
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
		ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, 
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
		ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, 
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
 *      | scope
 *      | base_type identifier "(" [ { base_type "," } base_type ] ")" ";"
 *      | base_type identifier "(" [ { base_type identifier [ "=" exp ] "," } 
 *        base_type identifier [ "=" exp ] ] ")" ( commaStmt | scope ) 
 *      | base_type identifier [ "=" exp ] ";" a
 *      | identifier ":"
 *      | exp ";"
 *      | ";" 
 */
u8 ba_PStmt(struct ba_Controller* ctr) {
	u64 firstLine = ctr->lex->line;
	u64 firstCol = ctr->lex->colStart;

	// "write" ...
	if (ba_PAccept(BA_TK_KW_WRITE, ctr)) {
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
			return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col);
		}

		if (stkItem->lexemeType == BA_TK_LITSTR) {
			str = ((struct ba_Str*)stkItem->val)->str;
			len = ((struct ba_Str*)stkItem->val)->len;
		}
		// Everything is printed as unsigned, this will be removed in the 
		// future anyway so i don't care about adding signed representation
		else if (ba_IsTypeIntegral(stkItem->type)) {
			if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
				str = ba_U64ToStr((u64)stkItem->val);
				len = strlen(str);
			}
			else {
				// U64ToStr is needed
				if (!ba_BltinFlagsTest(BA_BLTIN_U64ToStr)) {
					ba_BltinU64ToStr(ctr);
				}

				if (stkItem->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_DATASGMT, ((struct ba_STVal*)stkItem->val)->address);
				}
				else if (stkItem->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_ADRADD, BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, stkItem->val));
				}
				else if (stkItem->lexemeType == BA_TK_IMREGISTER) {
					if ((u64)stkItem->val != BA_IM_RAX) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, 
							(u64)stkItem->val);
					}
				}
				/* stkItem->lexemeType won't ever be BA_TK_IMRBPSUB */

				ba_AddIM(&ctr->im, 2, BA_IM_LABELCALL, 
					ba_BltinLabels[BA_BLTIN_U64ToStr]);

				// write
				ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RAX);
				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
				ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
				ba_AddIM(&ctr->im, 1, BA_IM_SYSCALL);

				// deallocate stack memory
				ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 0x38);

				// ... ';'
				ba_PExpect(';', ctr);

				return 1;
			}
		}
		else {
			return ba_ExitMsg(BA_EXIT_ERR, "expression of incorrect type "
				"used with 'write' keyword on", line, col);
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
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col);
			}

			u64 lblId = ctr->labelCnt++;
			u64 reg = BA_IM_RAX;

			if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
				if (!ba_IsTypeNumeric(stkItem->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "cannot use non-numeric literal "
						"as condition on", line, col);
				}

				if (stkItem->val) {
					ba_ExitMsg(BA_EXIT_WARN, "condition will always "
						"be true on", line, col);
				}
				else {
					ba_ExitMsg(BA_EXIT_WARN, "condition will always "
						"be false on", line, col);
					ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, lblId);
				}
			}
			else {
				if (stkItem->lexemeType == BA_TK_IMREGISTER) {
					reg = (u64)stkItem->val;
				}
				else if (stkItem->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT,
						((struct ba_STVal*)stkItem->val)->address);
				}
				else if (stkItem->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_ADRADD, BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, stkItem->val));
				}
				ba_AddIM(&ctr->im, 3, BA_IM_TEST, reg, reg);
				ba_AddIM(&ctr->im, 2, BA_IM_LABELJZ, lblId);
			}
		
			if (!ba_PCommaStmt(ctr) && !ba_PScope(ctr)) {
				return 0;
			}

			if (ctr->lex->type == BA_TK_KW_ELIF || ctr->lex->type == BA_TK_KW_ELSE) {
				ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, endLblId);
			}
			
			ba_AddIM(&ctr->im, 2, BA_IM_LABEL, lblId);
			
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
		ba_AddIM(&ctr->im, 2, BA_IM_LABEL, endLblId);

		return 1;
	}
	// "while" ...
	else if (ba_PAccept(BA_TK_KW_WHILE, ctr)) {
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		u64 startLblId = ctr->labelCnt++;
		ba_AddIM(&ctr->im, 2, BA_IM_LABEL, startLblId);

		// ... exp ...
		if (!ba_PExp(ctr)) {
			return 0;
		}

		struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
		if (!stkItem) {
			return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col);
		}

		u64 endLblId = ctr->labelCnt++;
		u64 reg = BA_IM_RAX;

		if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
			if (!ba_IsTypeNumeric(stkItem->type)) {
				return ba_ExitMsg(BA_EXIT_ERR, "cannot use non-numeric literal "
					"as while loop condition on", line, col);
			}

			if (!stkItem->val) {
				ba_ExitMsg(BA_EXIT_WARN, "while loop condition will always "
					"be false on", line, col);
				ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, endLblId);
			}
		}
		else {
			if (stkItem->lexemeType == BA_TK_IMREGISTER) {
				reg = (u64)stkItem->val;
			}
			else if (stkItem->lexemeType == BA_TK_GLOBALID) {
				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT,
					((struct ba_STVal*)stkItem->val)->address);
			}
			else if (stkItem->lexemeType == BA_TK_LOCALID) {
				ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
					BA_IM_ADRADD, BA_IM_RSP, 
					ba_CalcSTValOffset(ctr->currScope, stkItem->val));
			}
			ba_AddIM(&ctr->im, 3, BA_IM_TEST, reg, reg);
			ba_AddIM(&ctr->im, 2, BA_IM_LABELJZ, endLblId);
		}
		
		// ... ( commaStmt | scope ) ...
		ba_StkPush(ctr->breakLblStk, (void*)endLblId);
		if (!ba_PCommaStmt(ctr) && !ba_PScope(ctr)) {
			return 0;
		}
		ba_StkPop(ctr->breakLblStk);

		ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, startLblId);
		ba_AddIM(&ctr->im, 2, BA_IM_LABEL, endLblId);

		return 1;
	}
	// "break" ";"
	else if (ba_PAccept(BA_TK_KW_BREAK, ctr)) {
		if (!ctr->breakLblStk->count) {
			return ba_ExitMsg(BA_EXIT_ERR, "keyword 'break' used outside of "
				"loop on", firstLine, firstCol);
		}
		ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, (u64)ba_StkTop(ctr->breakLblStk));
		return ba_PExpect(';', ctr);
	}
	// "return" [ exp ] ";"
	else if (ba_PAccept(BA_TK_KW_RETURN, ctr)) {
		if (!ctr->currFunc) {
			return ba_ExitMsg(BA_EXIT_ERR, "keyword 'return' used outside of "
				"function on", firstLine, firstCol);
		}
		if (!ctr->lex) {
			ba_PExpect(';', ctr);
		}

		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;

		if (ba_PExp(ctr)) {
			if (ctr->currFunc->retType == BA_TYPE_VOID) {
				return ba_ExitMsg(BA_EXIT_ERR, "returning value from func with "
					"return type 'void' on", line, col);
			}

			// Move return value into rax
			// TODO: or onto stack if too big
			
			struct ba_PTkStkItem* stkItem = ba_StkPop(ctr->pTkStk);
			if (!stkItem) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", line, col);
			}
			if (stkItem->lexemeType == BA_TK_LITSTR) {
				return ba_ExitMsg(BA_EXIT_ERR, "returning string literal from "
					"a function currently not implemented,", line, col);
			}
			if (ba_IsTypeIntegral(stkItem->type)) {
				if (ba_IsLexemeLiteral(stkItem->lexemeType)) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_IMM, (u64)stkItem->val);
				}
				else if (stkItem->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_DATASGMT, ((struct ba_STVal*)stkItem->val)->address);
				}
				else if (stkItem->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_ADRADD, BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, stkItem->val));
				}
				else if (stkItem->lexemeType == BA_TK_IMREGISTER) {
					ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX,
						(u64)stkItem->val);
				}
				/* stkItem->lexemeType won't ever be BA_TK_IMRBPSUB */
			}
		}
		else if (ctr->currFunc->retType != BA_TYPE_VOID) {
			return ba_ExitMsg(BA_EXIT_ERR, "returning without value from func "
				"that does not have return type 'void' on", line, col);
		}
		ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, ctr->currFunc->lblEnd);
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
			ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, lblId);
		}
		else {
			ba_AddIM(&ctr->im, 4, BA_IM_GOTO, (u64)lblName, line, col);
		}
		
		return ba_PExpect(';', ctr);
	}
	// scope
	else if (ba_PScope(ctr)) {
		return 1;
	}
	// base_type identifier ...
	else if (ba_PBaseType(ctr)) {
		u64 type = ba_GetTypeFromKeyword(
			((struct ba_PTkStkItem*)ba_StkPop(ctr->pTkStk))->lexemeType);

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
		// ... [ "=" exp ] ";"
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
			return ba_ErrorVarRedef(lblName, line, col);
		}

		u64 lblId = ctr->labelCnt++;
		ba_AddIM(&ctr->im, 2, BA_IM_LABEL, lblId);
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
		ba_AddIM(&ctr->im, 1, BA_IM_NOP);
		return 1;
	}

	return 0;
}

/* program = { stmt } eof */
u8 ba_Parse(struct ba_Controller* ctr) {
	while (!ba_PAccept(BA_TK_EOF, ctr)) {
		if (!ba_PStmt(ctr)) {
			// doesn't have anything to do with literals, this is just 
			// a decent buffer size
			char msg[BA_LITERAL_SIZE+1];
			sprintf(msg, "unexpected %s at", ba_GetLexemeStr(ctr->lex->type));
			return ba_ExitMsg(BA_EXIT_ERR, msg, ctr->lex->line, ctr->lex->colStart);
		}
	}
	
	// Exit
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 60);
	ba_AddIM(&ctr->im, 3, BA_IM_XOR, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(&ctr->im, 1, BA_IM_SYSCALL);

	return 1;
}

#endif
