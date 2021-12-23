// See LICENSE for copyright/license information

#ifndef BA__PARSER_H
#define BA__PARSER_H

#include "lexer.h"
#include "bltin/bltin.h"
#include "parser_op.h"

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

/* base_type = "u64" | "i64" */
u8 ba_PBaseType(struct ba_Controller* ctr) {
	u64 lexType = ctr->lex->type;
	if (ba_PAccept(BA_TK_KW_U64, ctr) || ba_PAccept(BA_TK_KW_I64, ctr)) {
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

// Operator precedence
u8 ba_POpPrecedence(struct ba_POpStkItem* op) {
	if (!op) {
		return 255;
	}

	switch (op->syntax) {
		case BA_OP_PREFIX:
			if (op->lexemeType == '+' || op->lexemeType == '-' || 
				op->lexemeType == '!' || op->lexemeType == '~' ||
				op->lexemeType == '$' || 
				op->lexemeType == BA_TK_INC || op->lexemeType == BA_TK_DEC)
			{
				return 1;
			}
			else if (op->lexemeType == '(') {
				return 99;
			}
			break;

		case BA_OP_INFIX:
			if (op->lexemeType == BA_TK_LSHIFT ||
				op->lexemeType == BA_TK_RSHIFT)
			{
				return 2;
			}
			else if (op->lexemeType == '*' || op->lexemeType == '%' || 
				op->lexemeType == BA_TK_IDIV)
			{
				return 3;
			}
			else if (op->lexemeType == '&') {
				return 4;
			}
			else if (op->lexemeType == '^') {
				return 5;
			}
			else if (op->lexemeType == '|') {
				return 6;
			}
			else if (op->lexemeType == '+' || op->lexemeType == '-') {
				return 7;
			}
			else if (ba_IsLexemeCompare(op->lexemeType)) {
				return 8;
			}
			else if (op->lexemeType == BA_TK_LOGAND) {
				return 9;
			}
			else if (op->lexemeType == BA_TK_LOGOR) {
				return 10;
			}
			else if (op->lexemeType == '=' || 
				ba_IsLexemeCompoundAssign(op->lexemeType))
			{
				return 11;
			}
			break;

		case BA_OP_POSTFIX:
			if (op->lexemeType == ')') {
				return 100;
			}
			else if (op->lexemeType == '~') {
				return 0;
			}
			break;
	}
	
	fprintf(stderr, "Error: unknown operator with lexeme type 0x%llx, "
		"syntax type %u\n", op->lexemeType, op->syntax);
	exit(-1);
	return 255;
}

// Right associative operators don't handle operators of the same precedence
u8 ba_POpIsRightAssoc(struct ba_POpStkItem* op) {
	if (op->syntax == BA_OP_INFIX) {
		return op->lexemeType == '=' || ba_IsLexemeCompoundAssign(op->lexemeType);
	}
	else if (op->syntax == BA_OP_PREFIX) {
		return op->lexemeType == '(';
	}
	return 0;
}

// Handle operations (i.e. perform operation now or generate code for it)
u8 ba_POpHandle(struct ba_Controller* ctr, struct ba_POpStkItem* handler) {
	struct ba_POpStkItem* op = ba_StkPop(ctr->pOpStk);
	struct ba_PTkStkItem* arg = ba_StkPop(ctr->pTkStk);
	if (!arg) {
		return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, op->col);
	}
	
	switch (op->syntax) {
		case BA_OP_PREFIX:
		{
			if (op->lexemeType == '+') {
				if (!ba_IsTypeNumeric(arg->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "unary '+' used with non numeric "
						"operand on", op->line, op->col);
				}

				if (ba_IsTypeUnsigned(arg->type)) {
					arg->type = BA_TYPE_U64;
				}
				else if (ba_IsTypeSigned(arg->type)) {
					arg->type = BA_TYPE_I64;
				}
				
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			else if (op->lexemeType == '$') {
				arg->val = (void*)ba_GetSizeOfType(arg->type);

				if (!arg->val) {
					return ba_ExitMsg(BA_EXIT_ERR, "type of operand to '$' "
						"operator has undefined size on", op->line, op->col);
				}

				arg->type = BA_TYPE_U64;
				arg->lexemeType = BA_TK_LITINT;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			else if (op->lexemeType == '-' || op->lexemeType == '~' ||
				op->lexemeType == '!')
			{
				if ((op->lexemeType == '~' && !ba_IsTypeIntegral(arg->type)) ||
					!ba_IsTypeNumeric(arg->type))
				{
					fprintf(stderr, "Error: unary '%s' used with non %s operand "
						"on line %llu:%llu", ba_GetLexemeStr(op->lexemeType), 
						op->lexemeType == '~' ? "integral" : "numeric", 
						op->line, op->col);
					exit(-1);
				}

				u64 imOp;
				((op->lexemeType == '-') && (imOp = BA_IM_NEG)) ||
				((op->lexemeType == '~') && (imOp = BA_IM_NOT));

				if (ba_IsTypeIntegral(arg->type)) {
					if (ba_IsLexemeLiteral(arg->lexemeType)) {
						((op->lexemeType == '-') && 
							(arg->val = (void*)(-(u64)arg->val))) ||
						((op->lexemeType == '~') &&
							(arg->val = (void*)(~(u64)arg->val))) ||
						((op->lexemeType == '!') &&
							(arg->val = (void*)((u64)!arg->val)));
					}
					else {
						ba_POpNonLitUnary(op->lexemeType, arg, ctr);
					}
				}

				if (op->lexemeType == '!') {
					arg->type = BA_TYPE_U8;
				}
				else if (ba_IsTypeUnsigned(arg->type)) {
					arg->type = BA_TYPE_U64;
				}
				else if (ba_IsTypeSigned(arg->type)) {
					arg->type = BA_TYPE_I64;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			// Note: assumes arg is an identifier
			else if (op->lexemeType == BA_TK_INC || 
				op->lexemeType == BA_TK_DEC)
			{
				if (!arg->isLValue || (arg->lexemeType != BA_TK_GLOBALID && 
					arg->lexemeType != BA_TK_LOCALID))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "increment/decrement of "
						"non-lvalue on", op->line, op->col);
				}
				
				if (!ba_IsTypeNumeric(arg->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "increment of non-numeric"
						"lvalue on", op->line, op->col);
				}

				u64 imOp = op->lexemeType == BA_TK_INC ? BA_IM_INC : BA_IM_DEC;
				u64 stackPos = 0;
				u64 reg = ba_NextIMRegister(ctr);
				
				if (!reg) {
					if (!ctr->imStackCnt) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
					}
					++ctr->imStackCnt;
					ctr->imStackSize += 8;
					stackPos = ctr->imStackSize;

					// First: result location, second: preserve rcx
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
				}

				if (arg->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, reg ? reg : BA_IM_RAX,
						BA_IM_DATASGMT, ((struct ba_STVal*)arg->val)->address);
					ba_AddIM(&ctr->im, 2, imOp, reg ? reg : BA_IM_RAX);
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						((struct ba_STVal*)arg->val)->address, 
						reg ? reg : BA_IM_RAX);
				}
				else {
					u64 ofst = ba_CalcSTValOffset(ctr->currScope, arg->val);
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, reg ? reg : BA_IM_RAX,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, ofst);
					ba_AddIM(&ctr->im, 2, imOp, reg ? reg : BA_IM_RAX);
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRADD, 
						ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ofst, reg ? reg : BA_IM_RAX);
				}

				if (reg) {
					arg->lexemeType = BA_TK_IMREGISTER;
					arg->val = (void*)reg;
				}
				else {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
						stackPos, BA_IM_RAX);
					ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
					ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);

					arg->lexemeType = BA_TK_IMRBPSUB;
					arg->val = (void*)ctr->imStackSize;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			else if (op->lexemeType == '(') {
				ba_StkPush(ctr->pTkStk, arg);
				// IMPORTANT!!!!
				// This leads to the parser moving back to parsing lexemes 
				// as if it had just followed an atom, which is essentially 
				// what a grouped expression is
				return 2;
			}
			break;
		}

		case BA_OP_INFIX:
		{
			struct ba_PTkStkItem* lhs = ba_StkPop(ctr->pTkStk);
			if (!lhs) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, op->col);
			}
			struct ba_PTkStkItem* rhs = arg;
			
			u8 isLhsLiteral = ba_IsLexemeLiteral(lhs->lexemeType);
			u8 isRhsLiteral = ba_IsLexemeLiteral(rhs->lexemeType);

			// Bit shifts

			if (op->lexemeType == BA_TK_LSHIFT || 
				op->lexemeType == BA_TK_RSHIFT) 
			{
				if (!ba_IsTypeIntegral(lhs->type) || !ba_IsTypeIntegral(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift used with "
						"non integral operand(s) on", op->line, op->col);
				}

				arg->type = ba_IsTypeUnsigned(lhs->type) ? BA_TYPE_U64 : BA_TYPE_I64;
				
				// If rhs is 0, there is no change in in lhs
				if (isRhsLiteral && !rhs->val) {
					ba_StkPush(ctr->pTkStk, lhs);
					goto BA_LBL_OPHANDLE_END;
				}

				if (isLhsLiteral && isRhsLiteral) {
					// If both are literals
					arg->val = (op->lexemeType == BA_TK_LSHIFT)
						? (void*)(((u64)lhs->val) << ((u64)rhs->val & 0x3f))
						: (void*)(((u64)lhs->val) >> ((u64)rhs->val & 0x3f));
				}
				else {
					// Handle non literal bit shifts
					u64 imOp = op->lexemeType == BA_TK_LSHIFT ? BA_IM_SHL : BA_IM_SAR;
					ba_POpNonLitBitShift(imOp, arg, lhs, rhs, isRhsLiteral, ctr);
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			
			// Multiplication/addition/subtraction
			
			else if (op->lexemeType == '*' || op->lexemeType == '+' || 
				op->lexemeType == '-') 
			{
				char* opName = 0;
				u64 imOp = 0;

				if (op->lexemeType == '*') {
					opName = "multiplication";
					imOp = BA_IM_IMUL;
				}
				else if (op->lexemeType == '+') {
					opName = "addition";
					imOp = BA_IM_ADD;
				}
				else if (op->lexemeType == '-') {
					opName = "subtraction";
					imOp = BA_IM_SUB;
				}

				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					char msg[128];
					strcat(msg, opName);
					strcat(msg, " with non numeric operand(s) on");
					return ba_ExitMsg(BA_EXIT_ERR, msg, op->line, op->col);
				}

				if (ba_IsTypeIntegral(lhs->type) && ba_IsTypeIntegral(rhs->type)) {
					u64 argType = BA_TYPE_I64; // Default, most likely type
					if (ba_IsTypeUnsigned(lhs->type) ^ ba_IsTypeUnsigned(rhs->type)) {
						// Different signedness
						char msg[128];
						strcat(msg, opName);
						strcat(msg, " of integers of different signedness on");
						char* msgAfter = ba_IsWarningsAsErrors ? ""
							: ", implicitly converted lhs to i64";
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, msg, 
							op->line, op->col, msgAfter);
					}
					else if (ba_IsTypeUnsigned(lhs->type)) {
						argType = BA_TYPE_U64;
					}
					arg->type = argType;
				}

				if (isLhsLiteral && isRhsLiteral) {
					u64 argVal = 0;
					((op->lexemeType == '*') && 
						(argVal = (u64)lhs->val * (u64)rhs->val)) ||
					((op->lexemeType == '+') && 
						(argVal = (u64)lhs->val + (u64)rhs->val)) ||
					((op->lexemeType == '-') && 
						(argVal = (u64)lhs->val - (u64)rhs->val));
					arg->val = (void*)argVal;
				}
				// Multiplication optimizations
				else if ((op->lexemeType == '*') && (isLhsLiteral || isRhsLiteral)) {
					// Since multiplication is commutative
					struct ba_PTkStkItem* litArg = isLhsLiteral ? lhs : rhs;
					struct ba_PTkStkItem* nonLitArg = isLhsLiteral ? rhs : lhs;
					if (!litArg->val) {
						arg->val = (void*)0;
						arg->lexemeType = BA_TK_LITINT;
					}
					else if ((u64)litArg->val == 1) {
						arg->val = nonLitArg->val;
						arg->lexemeType = nonLitArg->lexemeType;
					}
					else if ((u64)litArg->val == 2) {
						ba_POpNonLitBinary(BA_IM_ADD, arg, nonLitArg, nonLitArg, 
							/* isLhsLiteral = */ 0, /* isRhsLiteral = */ 0, 
							/* isShortCirc = */ 0, ctr);
					}
					// If literal arg is a power of 2, generate a bit shift instead
					else if (!((u64)litArg->val & ((u64)litArg->val - 1))) {
						// NOTE: builtin may break outside of gcc
						// TODO: Replace with use of inline assembly TZCNT
						u64 shift = __builtin_ctzll((u64)litArg->val);

						struct ba_PTkStkItem* newRhs = malloc(sizeof(*newRhs));
						if (!newRhs) {
							return ba_ErrorMallocNoMem();
						}
						newRhs->val = (void*)shift;
						newRhs->type = BA_TYPE_U64;
						newRhs->lexemeType = BA_TK_LITINT;

						ba_POpNonLitBitShift(BA_IM_SHL, arg, nonLitArg, newRhs, 
							/* isRhsLiteral = */ 1, ctr);
					}
					else {
						goto BA_LBL_POPHANDLE_MULADDSUB_NONLIT;
					}
				}
				else {
					BA_LBL_POPHANDLE_MULADDSUB_NONLIT:
					ba_POpNonLitBinary(imOp, arg, lhs, rhs, isLhsLiteral, 
						isRhsLiteral, /* isShortCirc = */ 0, ctr);
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}

			// Division

			else if (op->lexemeType == BA_TK_IDIV) {
				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "integer division with "
						"non numeric operand(s) on", op->line, op->col);
				}

				u64 argType = BA_TYPE_I64; // Default, most likely type
				u8 areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
					ba_IsTypeUnsigned(rhs->type);

				if (areBothUnsigned) {
					argType = BA_TYPE_U64;
				}
				else if (ba_IsTypeUnsigned(lhs->type) ^ 
					ba_IsTypeUnsigned(rhs->type)) 
				{
					// Different signedness
					char* msgAfter = ba_IsWarningsAsErrors ? ""
						: ", implicitly converted lhs to i64";
					ba_ExitMsg2(BA_EXIT_EXTRAWARN, "integer division of numbers of "
						"different signedness on", op->line, op->col, msgAfter);
				}

				arg->type = argType;

				if (isLhsLiteral && isRhsLiteral) {
					arg->val = areBothUnsigned 
						? (void*)(((u64)lhs->val) / ((u64)rhs->val))
						: (void*)(((i64)lhs->val) / ((i64)rhs->val));
				}
				else if (isRhsLiteral) {
					if (!rhs->val) {
						return ba_ExitMsg(BA_EXIT_ERR, "division by zero on", 
							op->line, op->col);
					}

					u8 isRhsNeg = ba_IsTypeSigned(rhs->type) && (i64)rhs->val < 0;

					if (isRhsNeg) {
						ba_POpNonLitUnary('-', lhs, ctr);
						rhs->val = (void*)-(i64)rhs->val;
					}

					if ((u64)rhs->val == 1) {
						arg->val = lhs->val;
						arg->lexemeType = lhs->lexemeType;
					}
					/* TODO: complete this optimization:
					 * if lhs is negative, then the following optimization is only 
					 * correct if (lhs & ((1 << shift) - 1)) == 0
					// If literal arg is a power of 2, generate a bit shift instead
					else if (!((u64)rhs->val & ((u64)rhs->val - 1))) {
						// NOTE: builtin may break outside of gcc
						// TODO: Replace with use of inline assembly TZCNT
						u64 shift = __builtin_ctzll((u64)rhs->val);

						struct ba_PTkStkItem* newRhs = malloc(sizeof(*newRhs));
						if (!newRhs) {
							return ba_ErrorMallocNoMem();
						}
						newRhs->val = (void*)shift;
						newRhs->type = BA_TYPE_U64;
						newRhs->lexemeType = BA_TK_LITINT;

						ba_POpNonLitBitShift(BA_IM_SAR, arg, lhs, newRhs, 
							/ * isRhsLiteral = * / 1, ctr);
					}
					*/
					else {
						goto BA_LBL_POPHANDLE_INTDIV_NONLIT;
					}
				}
				else {
					BA_LBL_POPHANDLE_INTDIV_NONLIT:
					
					u64 lhsStackPos = 0;
					u64 regL = (u64)lhs->val; // Kept only if lhs is a register
					u64 regR = (u64)rhs->val; // Kept only if rhs is a register

					// Return 0 means on the stack
					if (lhs->lexemeType != BA_TK_IMREGISTER) {
						regL = ba_NextIMRegister(ctr);
					}

					if (!regL) {
						if (!ctr->imStackCnt) {
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
						}
						++ctr->imStackCnt;
						ctr->imStackSize += 8;
						lhsStackPos = ctr->imStackSize;
						
						// Result location
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
					}

					u64 originalUsedRaxRdx = ctr->usedRegisters & 
						(BA_CTRREG_RAX | BA_CTRREG_RDX);
					ctr->usedRegisters |= BA_CTRREG_RAX | BA_CTRREG_RDX;

					if (rhs->lexemeType != BA_TK_IMREGISTER || 
						(u64)rhs->val == BA_IM_RAX || (u64)rhs->val == BA_IM_RDX)
					{
						regR = ba_NextIMRegister(ctr);
					}

					ctr->usedRegisters &= 
						(ctr->usedRegisters & ~BA_CTRREG_RAX & ~BA_CTRREG_RDX) | 
						originalUsedRaxRdx;

					if ((ctr->usedRegisters & BA_CTRREG_RAX) && (regL != BA_IM_RAX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, regR, BA_IM_RAX);
						}
						else {
							ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
						}
					}

					if ((ctr->usedRegisters & BA_CTRREG_RDX) && (regL != BA_IM_RDX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, regR, BA_IM_RDX);
						}
						else {
							ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RDX);
						}
					}

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
					}
					else {
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RCX);
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
					}
					else if (lhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, lhs->val));
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRSUB, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
							(u64)lhs->val);
					}
					else if (isLhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_IMM, (u64)lhs->val);
					}

					if (rhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
					}
					else if (rhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, rhs->val));
					}
					else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
					}
					else if (isRhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_IMM, (u64)rhs->val);
					}

					ba_AddIM(&ctr->im, 1, BA_IM_CQO);

					u64 imOp = areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV;
					ba_AddIM(&ctr->im, 2, imOp, regR ? regR : BA_IM_RCX);

					if (regL) {
						if (regL != BA_IM_RAX) {
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, regL, BA_IM_RAX);
						}
						arg->lexemeType = BA_TK_IMREGISTER;
						arg->val = (void*)regL;
					}
					else {
						ba_AddIM(&ctr->im, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
							lhsStackPos, BA_IM_RAX);
						arg->lexemeType = BA_TK_IMRBPSUB;
						arg->val = (void*)ctr->imStackSize;
					}

					if ((ctr->usedRegisters & BA_CTRREG_RDX) && (regL != BA_IM_RDX)) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RDX);
					}

					if (lhs->lexemeType == BA_TK_IMREGISTER) {
						ctr->usedRegisters &= ~BA_CTRREG_RDX;
					}

					if (!regR) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RCX);
					}

					if ((ctr->usedRegisters & BA_CTRREG_RAX) && (regL != BA_IM_RAX)) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
					}
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}

			// Modulo

			else if (op->lexemeType == '%') {
				/* Modulo in Basque is from floored divison (like in Python) */

				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "modulo with "
						"non numeric operand(s) on", op->line, op->col);
				}

				u64 argType = BA_TYPE_I64; // Default, most likely type
				u8 areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
					ba_IsTypeUnsigned(rhs->type);

				if (areBothUnsigned) {
					argType = BA_TYPE_U64;
				}
				else if (ba_IsTypeUnsigned(lhs->type) ^ 
					ba_IsTypeUnsigned(rhs->type)) 
				{
					// Different signedness
					char* msgAfter = ba_IsWarningsAsErrors ? ""
						: ", implicitly converted lhs to i64";
					ba_ExitMsg2(BA_EXIT_EXTRAWARN, "modulo of numbers of "
						"different signedness on", op->line, op->col, msgAfter);
				}

				arg->type = argType;

				if (isLhsLiteral && isRhsLiteral) {
					u64 modVal = areBothUnsigned 
						? (u64)lhs->val % (u64)rhs->val
						: (i64)lhs->val % (i64)rhs->val;
						
					modVal += (i64)rhs->val *
						(!areBothUnsigned && 
							(((i64)lhs->val < 0) ^ ((i64)rhs->val < 0)));

					arg->val = (void*)modVal;
				}
				else if (isRhsLiteral) {
					if (!rhs->val) {
						return ba_ExitMsg(BA_EXIT_ERR, "modulo by zero on", 
							op->line, op->col);
					}

					u8 isRhsNeg = ba_IsTypeSigned(rhs->type) && (i64)rhs->val < 0;
					u64 rhsAbs = isRhsNeg ? -(u64)rhs->val : (u64)rhs->val;

					if (rhsAbs == 1) {
						arg->val = 0;
						arg->lexemeType = BA_TK_LITINT;
					}
					else if (!(rhsAbs & (rhsAbs - 1))) {
						/* a % (1<<b) == a & ((1<<b)-1)
						 * a % -(1<<b) == (a & ((1<<b)-1)) - (1<<b) */

						struct ba_PTkStkItem* newRhs = malloc(sizeof(*newRhs));
						if (!newRhs) {
							return ba_ErrorMallocNoMem();
						}
						newRhs->val = (void*)(rhsAbs-1);
						newRhs->type = BA_TYPE_U64;
						newRhs->lexemeType = BA_TK_LITINT;

						ba_POpNonLitBitShift(BA_IM_AND, arg, lhs, newRhs, 
							/* isRhsLiteral = */ 1, ctr);

						if (isRhsNeg) {
							newRhs->val = (void*)rhsAbs;
							newRhs->type = BA_TYPE_I64;
							newRhs->lexemeType = BA_TK_LITINT;
							ba_POpNonLitBinary(BA_IM_SUB, arg, arg, newRhs,
								/* isLhsLiteral = */ 0, /* isRhsLiteral = */ 1,
								/* isShortCirc = */ 0, ctr);
						}
					}
					else {
						goto BA_LBL_POPHANDLE_MODULO_NONLIT;
					}
				}
				else {
					BA_LBL_POPHANDLE_MODULO_NONLIT:
					
					u64 lhsStackPos = 0;
					u64 regL = (u64)lhs->val; // Kept only if lhs is a register
					u64 regR = (u64)rhs->val; // Kept only if rhs is a register

					// Return 0 means on the stack
					if (lhs->lexemeType != BA_TK_IMREGISTER) {
						regL = ba_NextIMRegister(ctr);
					}

					if (!regL) {
						if (!ctr->imStackCnt) {
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
						}
						++ctr->imStackCnt;
						ctr->imStackSize += 8;
						lhsStackPos = ctr->imStackSize;
						
						// Result location
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
					}

					u64 originalUsedRaxRdx = ctr->usedRegisters & 
						(BA_CTRREG_RAX | BA_CTRREG_RDX);
					ctr->usedRegisters |= BA_CTRREG_RAX | BA_CTRREG_RDX;

					if (rhs->lexemeType != BA_TK_IMREGISTER || 
						(u64)rhs->val == BA_IM_RAX || (u64)rhs->val == BA_IM_RDX)
					{
						regR = ba_NextIMRegister(ctr);
					}

					ctr->usedRegisters &= 
						(ctr->usedRegisters & ~BA_CTRREG_RAX & ~BA_CTRREG_RDX) | 
						originalUsedRaxRdx;

					if ((ctr->usedRegisters & BA_CTRREG_RAX) && (regL != BA_IM_RAX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, regR, BA_IM_RAX);
						}
						else {
							ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
						}
					}

					if ((ctr->usedRegisters & BA_CTRREG_RDX) && (regL != BA_IM_RDX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, regR, BA_IM_RDX);
						}
						else {
							ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RDX);
						}
					}

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
					}
					else {
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RCX);
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
					}
					else if (lhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, lhs->val));
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
					}
					else if (isLhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_IMM, (u64)lhs->val);
					}

					if (rhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
					}
					else if (rhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, rhs->val));
					}
					else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
					}
					else if (isRhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_IMM, (u64)rhs->val);
					}

					ba_AddIM(&ctr->im, 1, BA_IM_CQO);

					u64 imOp = areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV;
					ba_AddIM(&ctr->im, 2, imOp, regR ? regR : BA_IM_RCX);

					if (!areBothUnsigned) {
						// Test result for sign: if negative, then they had opposite
						// signs and so the rhs needs to be added
						
						// Sign stored in rax
						ba_AddIM(&ctr->im, 3, BA_IM_TEST, BA_IM_RAX, BA_IM_RAX);
						ba_AddIM(&ctr->im, 2, BA_IM_SETS, BA_IM_AL);
						ba_AddIM(&ctr->im, 3, BA_IM_MOVZX, BA_IM_RAX, BA_IM_AL);

						ba_AddIM(&ctr->im, 3, BA_IM_IMUL, BA_IM_RAX, 
							regR ? regR : BA_IM_RCX);
						ba_AddIM(&ctr->im, 3, BA_IM_ADD, BA_IM_RDX, BA_IM_RAX);
					}

					if (regL) {
						if (regL != BA_IM_RDX) {
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, regL, BA_IM_RDX);
						}
						arg->lexemeType = BA_TK_IMREGISTER;
						arg->val = (void*)regL;
					}
					else {
						ba_AddIM(&ctr->im, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
							lhsStackPos, BA_IM_RDX);
						arg->lexemeType = BA_TK_IMRBPSUB;
						arg->val = (void*)ctr->imStackSize;
					}

					if ((ctr->usedRegisters & BA_CTRREG_RDX) && (regL != BA_IM_RDX)) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RDX);
					}

					if (lhs->lexemeType == BA_TK_IMREGISTER) {
						ctr->usedRegisters &= ~BA_CTRREG_RDX;
					}

					if (!regR) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RCX);
					}

					if ((ctr->usedRegisters & BA_CTRREG_RAX) && (regL != BA_IM_RAX)) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
					}
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			
			// Bitwise operations
			
			else if (op->lexemeType == '&' || op->lexemeType == '^' ||
				op->lexemeType == '|')
			{
				if (!ba_IsTypeIntegral(lhs->type) && !ba_IsTypeIntegral(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "bitwise operation used with "
						"non integral operand(s) on", op->line, op->col);
				}

				u64 argType = BA_TYPE_I64; // Default, most likely type
				ba_IsTypeUnsigned(lhs->type) && ba_IsTypeUnsigned(rhs->type) &&
					(argType = BA_TYPE_U64);
				arg->type = argType;

				if (isLhsLiteral && isRhsLiteral) {
					u64 argVal = 0;
					((op->lexemeType == '&') && 
						(argVal = (u64)lhs->val & (u64)rhs->val)) ||
					((op->lexemeType == '^') && 
						(argVal = (u64)lhs->val ^ (u64)rhs->val)) ||
					((op->lexemeType == '|') && 
						(argVal = (u64)lhs->val | (u64)rhs->val));
					arg->val = (void*)argVal;
				}
				else {
					u64 imOp = 0;
					((op->lexemeType == '&') && (imOp = BA_IM_AND)) ||
					((op->lexemeType == '^') && (imOp = BA_IM_XOR)) ||
					((op->lexemeType == '|') && (imOp = BA_IM_OR));
					ba_POpNonLitBinary(imOp, arg, lhs, rhs, isLhsLiteral,
						isRhsLiteral, /* isShortCirc = */ 0, ctr);
				}
				
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			
			// Logical short-circuiting operators
			else if (op->lexemeType == BA_TK_LOGAND || 
				op->lexemeType == BA_TK_LOGOR) 
			{
				if (!ba_IsTypeNumeric(lhs->type) ||
					!ba_IsTypeNumeric(rhs->type))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "logical short-circuiting "
						"operation with non numeric operand(s) on", 
						op->line, op->col);
				}

				arg->type = BA_TYPE_U8;

				u64 imOp = op->lexemeType == BA_TK_LOGAND
					? BA_IM_AND : BA_IM_OR;

				// Although it is called "NonLit", it does work with literals 
				// as well. The reason for the name is that it isn't generally 
				// used for that.
				ba_POpNonLitBinary(imOp, arg, lhs, rhs, isLhsLiteral, 
					isRhsLiteral, /* isShortCirc = */ 1, ctr);
				
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}
			
			// Assignment
			// Note: assumes lhs is an identifier
			else if (op->lexemeType == '=' || 
				ba_IsLexemeCompoundAssign(op->lexemeType)) 
			{
				if (!lhs->isLValue || (lhs->lexemeType != BA_TK_GLOBALID &&
						lhs->lexemeType != BA_TK_LOCALID)) 
				{
					return ba_ExitMsg(BA_EXIT_ERR, "assignment to non-lvalue on", 
						op->line, op->col);
				}

				u64 opLex = op->lexemeType;

				if (!ba_IsTypeNumeric(lhs->type)) {
					if (lhs->type != rhs->type) {
						return ba_ExitMsg(BA_EXIT_ERR, "assignment of "
							"incompatible types on", op->line, op->col);
					}
				}
				else if (!ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "assignment of non-numeric "
						"expression to numeric lvalue on", op->line, op->col);
				}
				else if ((opLex == BA_TK_BITANDEQ || opLex == BA_TK_BITXOREQ || 
					opLex == BA_TK_BITOREQ || opLex == BA_TK_LSHIFTEQ || 
					opLex == BA_TK_RSHIFTEQ) && (!ba_IsTypeIntegral(lhs->type) || 
					!ba_IsTypeIntegral(rhs->type)))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift or bitwise operation "
						"used with non integral operand(s) on", op->line, op->col);
				}

				u64 stackPos = 0;
				u64 reg = (u64)rhs->val; // Kept only if rhs is a register
				if (rhs->lexemeType != BA_TK_IMREGISTER) {
					reg = ba_NextIMRegister(ctr);
				}
				
				if (!reg) {
					if (!ctr->imStackCnt) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
					}
					++ctr->imStackCnt;
					ctr->imStackSize += 8;
					stackPos = ctr->imStackSize;

					// First: result location, second: preserve rcx
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RCX);
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RCX);
				}

				if (rhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
				}
				else if (rhs->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, rhs->val));
				}
				else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
				}
				else if (rhs->lexemeType != BA_TK_IMREGISTER) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_IMM, (u64)rhs->val);
				}

				u8 areBothUnsigned = ba_IsTypeUnsigned(lhs->type) &&
					ba_IsTypeUnsigned(rhs->type);

				u64 imOp = 0;
				((opLex == BA_TK_ADDEQ) && (imOp = BA_IM_ADD)) ||
				((opLex == BA_TK_SUBEQ) && (imOp = BA_IM_SUB)) ||
				((opLex == BA_TK_MULEQ) && (imOp = BA_IM_IMUL)) ||
				((opLex == BA_TK_LSHIFTEQ) && (imOp = BA_IM_SHL)) ||
				((opLex == BA_TK_RSHIFTEQ) && (imOp = BA_IM_SAR)) ||
				((opLex == BA_TK_BITANDEQ) && (imOp = BA_IM_AND)) ||
				((opLex == BA_TK_BITXOREQ) && (imOp = BA_IM_XOR)) ||
				((opLex == BA_TK_BITOREQ) && (imOp = BA_IM_OR));

				u64 lhsOffset = 
					ba_CalcSTValOffset(ctr->currScope, lhs->val);

				if (opLex == BA_TK_IDIVEQ || opLex == BA_TK_MODEQ) {
					if (isRhsLiteral && !rhs->val) {
						return ba_ExitMsg(BA_EXIT_ERR, "division or modulo by "
							"zero on", op->line, op->col);
					}

					if (ctr->usedRegisters & BA_CTRREG_RAX) {
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
					}
					if (ctr->usedRegisters & BA_CTRREG_RDX) {
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RDX);
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT,
							((struct ba_STVal*)lhs->val)->address);
					}
					else {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
							lhsOffset);
					}

					ba_AddIM(&ctr->im, 1, BA_IM_CQO);
					ba_AddIM(&ctr->im, 2, areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV, 
						reg ? reg : BA_IM_RCX);
					
					u64 divResultReg = BA_TK_IDIVEQ ? BA_IM_RAX : BA_IM_RDX;
					if (reg != divResultReg) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, reg ? reg : BA_IM_RCX,
							divResultReg);
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
							((struct ba_STVal*)lhs->val)->address, 
							reg ? reg : BA_IM_RCX);
					}
					else {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRADD, 
							BA_IM_RBP, lhsOffset, reg ? reg : BA_IM_RCX);
					}

					if (ctr->usedRegisters & BA_CTRREG_RDX) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RDX);
					}
					if (ctr->usedRegisters & BA_CTRREG_RAX) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
					}
				}
				else if (imOp) {
					u64 opResultReg = reg == BA_IM_RAX ? BA_IM_RDX : BA_IM_RAX;
					u8 isPushOpResReg = 
						((reg == BA_IM_RAX && 
							(ctr->usedRegisters & BA_CTRREG_RDX)) ||
						(reg == BA_IM_RDX && 
							(ctr->usedRegisters & BA_CTRREG_RAX)));

					if (isPushOpResReg) {
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, opResultReg);
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, opResultReg, BA_IM_DATASGMT, 
							((struct ba_STVal*)lhs->val)->address);
					}
					else {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, opResultReg, BA_IM_ADRADD, 
							ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, lhsOffset);
					}

					if (opLex == BA_TK_LSHIFTEQ || opLex == BA_TK_RSHIFTEQ) {
						if (reg != BA_IM_RCX) {
							if (ctr->usedRegisters & BA_CTRREG_RCX) {
								ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RCX);
							}
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RCX, reg);
						}

						ba_AddIM(&ctr->im, 3, imOp, opResultReg, BA_IM_CL);

						if (reg != BA_IM_RCX && (ctr->usedRegisters & BA_CTRREG_RCX)) {
							ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RCX);
						}
					}
					else {
						ba_AddIM(&ctr->im, 3, imOp, opResultReg, 
							reg ? reg : BA_IM_RCX);
					}

					ba_AddIM(&ctr->im, 3, BA_IM_MOV, reg ? reg : BA_IM_RCX, 
						opResultReg);

					if (isPushOpResReg) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, opResultReg);
					}
				}

				if (lhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						((struct ba_STVal*)lhs->val)->address, 
						reg ? reg : BA_IM_RCX);
				}
				else {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRADD, 
						ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						lhsOffset, reg ? reg : BA_IM_RCX);
				}

				if (reg) {
					arg->lexemeType = BA_TK_IMREGISTER;
					arg->val = (void*)reg;
				}
				else {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
						stackPos, BA_IM_RCX);
					ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RCX);
					ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RCX);

					arg->lexemeType = BA_TK_IMRBPSUB;
					arg->val = (void*)ctr->imStackSize;
				}

				arg->type = lhs->type;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}

			// Comparison
			else if (ba_IsLexemeCompare(op->lexemeType)) {
				if (!ba_IsTypeNumeric(lhs->type) ||
					!ba_IsTypeNumeric(rhs->type))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "comparison operation "
						"with non numeric operand(s) on", op->line, op->col);
				}
				else if (ba_IsTypeUnsigned(lhs->type) ^ 
					ba_IsTypeUnsigned(rhs->type))
				{
					ba_ExitMsg(BA_EXIT_WARN, "comparison of integers of " 
						"different signedness on", op->line, op->col);
				}

				struct ba_PTkStkItem* rhsCopy = malloc(sizeof(*rhsCopy));
				memcpy(rhsCopy, rhs, sizeof(*rhsCopy));

				u64 opLex = op->lexemeType;
				u64 imOpSet = 0;
				u64 imOpJcc = 0;

				u8 areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
					ba_IsTypeUnsigned(rhs->type);

				if (opLex == '<') {
					imOpSet = areBothUnsigned ? BA_IM_SETB : BA_IM_SETL;
					imOpJcc = areBothUnsigned ? BA_IM_LABELJAE : BA_IM_LABELJGE;
				}
				else if (opLex == '>') {
					imOpSet = areBothUnsigned ? BA_IM_SETA : BA_IM_SETG;
					imOpJcc = areBothUnsigned ? BA_IM_LABELJBE : BA_IM_LABELJLE;
				}
				else if (opLex == BA_TK_LTE) {
					imOpSet = areBothUnsigned ? BA_IM_SETBE : BA_IM_SETLE;
					imOpJcc = areBothUnsigned ? BA_IM_LABELJA : BA_IM_LABELJG;
				}
				else if (opLex == BA_TK_GTE) {
					imOpSet = areBothUnsigned ? BA_IM_SETAE : BA_IM_SETGE;
					imOpJcc = areBothUnsigned ? BA_IM_LABELJB : BA_IM_LABELJL;
				}
				else if (opLex == BA_TK_DBEQUAL) {
					imOpSet = BA_IM_SETZ;
					imOpJcc = BA_IM_LABELJNZ;
				}
				else if (opLex == BA_TK_NEQUAL) {
					imOpSet = BA_IM_SETNZ;
					imOpJcc = BA_IM_LABELJZ;
				}

				arg->type = BA_TYPE_U8;
				arg->isLValue = 0;

				u64 lhsStackPos = 0;
				u64 regL = (u64)lhs->val; // Kept only if lhs is a register
				u64 regR = (u64)rhs->val; // Kept only if rhs is a register

				// Return 0 means on the stack
				(lhs->lexemeType != BA_TK_IMREGISTER) &&
					(regL = ba_NextIMRegister(ctr));
				(rhs->lexemeType != BA_TK_IMREGISTER) &&
					(regR = ba_NextIMRegister(ctr));

				if (!regL) {
					if (!ctr->imStackCnt) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, 
							BA_IM_RSP);
					}
					++ctr->imStackCnt;
					ctr->imStackSize += 8;
					lhsStackPos = ctr->imStackSize;

					// First: result location, second: preserve rax
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
				}
				
				/* regR is replaced with rdx normally, but if regL is 
				 * already rdx, regR will be set to rcx. */
				u64 realRegL = regL ? regL : BA_IM_RAX;
				u64 realRegR = regR ? regR : 
					(regL == BA_IM_RDX ? BA_IM_RCX : BA_IM_RDX);

				if (!regR) {
					// Only once to preserve rdx/rcx
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, realRegR);
				}

				if (lhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realRegL,
						BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
				}
				else if (lhs->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, realRegL,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, lhs->val));
				}
				else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, realRegL,
						BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
				}
				else if (isLhsLiteral) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realRegL,
						BA_IM_IMM, (u64)lhs->val);
				}

				if (rhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realRegR,
						BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
				}
				else if (rhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realRegR,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, rhs->val));
				}
				else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, realRegR,
						BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
				}
				else if (isRhsLiteral) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realRegR,
						BA_IM_IMM, (u64)rhs->val);
				}

				u64 movReg = (u64)ba_StkTop(ctr->cmpRegStk);
				!movReg && (movReg = realRegL);

				ba_AddIM(&ctr->im, 3, BA_IM_CMP, realRegL, realRegR);
				ba_AddIM(&ctr->im, 2, imOpSet, movReg - BA_IM_RAX + BA_IM_AL);
				ba_AddIM(&ctr->im, 3, BA_IM_MOVZX, movReg, 
					movReg - BA_IM_RAX + BA_IM_AL);

				if (regL) {
					arg->lexemeType = BA_TK_IMREGISTER;
					arg->val = (void*)realRegL;
				}
				else {
					arg->lexemeType = BA_TK_IMRBPSUB;
					arg->val = (void*)ctr->imStackSize;

					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
						lhsStackPos, BA_IM_RAX);
				}

				if (handler && ba_IsLexemeCompare(handler->lexemeType)) {
					// First in the chain
					if (!ba_StkTop(ctr->cmpRegStk)) {
						ba_StkPush(ctr->pTkStk, arg);
						ba_StkPush(ctr->cmpLblStk, (void*)(ctr->labelCnt++));
						ctr->cmpRegStk->items[ctr->cmpRegStk->count-1] = 
							(void*)realRegL;
					}
					else if (regL) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regL);
					}

					ba_StkPush(ctr->pTkStk, rhsCopy);

					ba_AddIM(&ctr->im, 2, imOpJcc, 
						(u64)ba_StkTop(ctr->cmpLblStk));
				}
				else {
					if (!ba_StkTop(ctr->cmpRegStk)) {
						ba_StkPush(ctr->pTkStk, arg);
					}
					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
					}
					else {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, realRegR);
					}

					if (ba_StkTop(ctr->cmpLblStk)) {
						ba_AddIM(&ctr->im, 2, BA_IM_LABEL, 
							(u64)ba_StkPop(ctr->cmpLblStk));
					}
					ctr->cmpRegStk->items[ctr->cmpRegStk->count-1] = (void*)0;
				}

				if (!regL) {
					ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
				}

				goto BA_LBL_OPHANDLE_END;
			}

			break;
		}

		case BA_OP_POSTFIX:
		{
			// This should never occur
			if (op->lexemeType == ')') {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, op->col);
			}
			else if (op->lexemeType == '~') {
				struct ba_PTkStkItem* castedExp = ba_StkPop(ctr->pTkStk);
				if (!castedExp) {
					return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", 
						op->line, op->col);
				}
				struct ba_PTkStkItem* typeArg = arg;

				u64 newType = ba_GetTypeFromKeyword(typeArg->lexemeType);

				if (!newType) {
					return ba_ExitMsg(BA_EXIT_ERR, "cast to expression that is "
						"not a type on", op->line, op->col);
				}

				if (ba_IsTypeNumeric(newType) != 
					ba_IsTypeNumeric(castedExp->type))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "cast with incompatible "
						"types on", op->line, op->col);
				}
				
				arg = castedExp;
				arg->type = newType;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				goto BA_LBL_OPHANDLE_END;
			}

			break;
		}
	}
	
	BA_LBL_OPHANDLE_END:;
	return 1;
}

// Any type of expression
u8 ba_PExp(struct ba_Controller* ctr) {
	// Reset comparison chain stacks
	ctr->cmpLblStk->count = 0;
	ctr->cmpRegStk->count = 1;
	ctr->cmpRegStk->items[0] = (void*)0;

	// Parse as if following an operator or parse as if following an atom?
	u8 isAfterAtom = 0;

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
			if (ba_PAccept(')', ctr)) {
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
					u8 willHandle = ba_POpIsRightAssoc(op)
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
 *      | "goto" identifier ";"
 *      | scope
 *      | base_type identifier [ "=" exp ] ";" 
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
				/* stkItem->lexemeType won't ever be BA_IM_RBPSUB */

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

		u8 hasReachedElse = 0;

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
	// while ...
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
			ba_ExitMsg(BA_EXIT_ERR, "keyword 'break' used outside of loop on",
				firstLine, firstCol);
		}
		ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, (u64)ba_StkTop(ctr->breakLblStk));
		return ba_PExpect(';', ctr);
	}
	// "goto" identifier ";"
	else if (ba_PAccept(BA_TK_KW_GOTO, ctr)) {
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

		struct ba_STVal* idVal = 
			ba_SymTableSearchChildren(ctr->globalST, idName);

		if (idVal) {
			if (idVal->type != BA_TYPE_LABEL) {
				return ba_ExitMsg(BA_EXIT_ERR, "identifier is not a label on", 
					line, col);
			}
			ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, idVal->address);
		}
		else {
			ba_AddIM(&ctr->im, 4, BA_IM_GOTO, (u64)idName, line, col);
		}
		
		return ba_PExpect(';', ctr);
	}
	// scope
	else if (ba_PScope(ctr)) {
		return 1;
	}
	// base_type identifier [ "=" exp ] ";"
	else if (ba_PBaseType(ctr)) {
		struct ba_PTkStkItem* varTypeItem = ba_StkPop(ctr->pTkStk);
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

		struct ba_STVal* idVal = ba_STGet(ctr->currScope, idName);
		
		if (idVal) {
			return ba_ErrorVarRedef(idName, line, col);
		}

		idVal = malloc(sizeof(struct ba_STVal));
		if (!idVal) {
			return ba_ErrorMallocNoMem();
		}
		idVal->scope = ctr->currScope;
		idVal->type = ba_GetTypeFromKeyword(varTypeItem->lexemeType);
		
		u64 idDataSize = ba_GetSizeOfType(idVal->type);
		/* For global identifiers, the address of the start is used 
		 * For non global identifiers, the address of the end is used */
		idVal->address = ctr->currScope->dataSize + 
			(ctr->currScope != ctr->globalST) * idDataSize;
		ctr->currScope->dataSize += idDataSize;

		idVal->initVal = 0;
		idVal->isInited = 0;

		ba_STSet(ctr->currScope, idName, idVal);

		if (ba_PAccept('=', ctr)) {
			idVal->isInited = 1;

			line = ctr->lex->line;
			col = ctr->lex->colStart;
			
			if (!ba_PExp(ctr)) {
				return 0;
			}
			struct ba_PTkStkItem* expItem = ba_StkPop(ctr->pTkStk);
			
			if (ba_IsTypeNumeric(idVal->type)) {
				if (!ba_IsTypeNumeric(expItem->type)) {
					char* expTypeStr = ba_GetTypeStr(expItem->type);
					char* varTypeStr = ba_GetTypeStr(varTypeItem->type);
					return ba_ErrorAssignTypes(expTypeStr, idName, 
						varTypeStr, line, col);
				}

				if (expItem->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT, 
						((struct ba_STVal*)expItem->val)->address);
				}
				else if (expItem->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, expItem->val));
				}

				if (ctr->currScope == ctr->globalST) {
					if (ba_IsLexemeLiteral(expItem->lexemeType)) {
						idVal->initVal = expItem->val;
					}
					else {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
							idVal->address, 
							expItem->lexemeType == BA_TK_IMREGISTER ? 
								(u64)expItem->val : BA_IM_RAX);
					}
				}
				else {
					if (ba_IsLexemeLiteral(expItem->lexemeType)) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, 
							BA_IM_IMM, (u64)expItem->val);
					}
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH,
						expItem->lexemeType == BA_TK_IMREGISTER ? 
							(u64)expItem->val : BA_IM_RAX);
				}
			}
		}

		if (!ba_PExpect(';', ctr)) {
			return 0;
		}

		if (ctr->currScope != ctr->globalST && !idVal->isInited) {
			ba_AddIM(&ctr->im, 3, BA_IM_PUSH, BA_IM_IMM, 0);
		}
		
		return 1;
	}
	// identifier ":"
	else if (ctr->lex && ctr->lex->type == BA_TK_IDENTIFIER && 
		ctr->lex->next && ctr->lex->next->type == ':')
	{
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

		ba_PExpect(BA_TK_IDENTIFIER, ctr);
		ba_PExpect(':', ctr);

		struct ba_STVal* idVal = 
			ba_SymTableSearchChildren(ctr->globalST, idName);
		
		if (idVal) {
			return ba_ErrorVarRedef(idName, line, col);
		}

		idVal = malloc(sizeof(struct ba_STVal));
		if (!idVal) {
			return ba_ErrorMallocNoMem();
		}

		idVal->type = BA_TYPE_LABEL;
		idVal->address = ctr->labelCnt++;
		idVal->initVal = 0;
		idVal->isInited = 1;

		ba_AddIM(&ctr->im, 2, BA_IM_LABEL, idVal->address);

		ba_STSet(ctr->currScope, idName, idVal);

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

		// TODO
		
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
