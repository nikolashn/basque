// See LICENSE for copyright/license information

#ifndef BA__PARSER_H
#define BA__PARSER_H

#include "lexer.h"
#include "bltin/bltin.h"
#include "parser_op.h"

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
			printf("Error: expected %s at end of file\n", 
				ba_GetLexemeStr(type));
		}
		else {
			printf("Error: expected %s at line %llu:%llu\n",
				ba_GetLexemeStr(type), ctr->lex->line, ctr->lex->colStart);
		}
		exit(-1);
		return 0;
	}
	return 1;
}

/* base_type = "u64" | "i64" */
u8 ba_PBaseType(struct ba_Controller* ctr) {
	if (ba_PAccept(BA_TK_KW_U64, ctr)) {
		ba_PTkStkPush(ctr, 0, BA_TYPE_TYPE, BA_TK_KW_U64);
	}
	else if (ba_PAccept(BA_TK_KW_I64, ctr)) {
		ba_PTkStkPush(ctr, 0, BA_TYPE_TYPE, BA_TK_KW_I64);
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

			str = realloc(str, len);
			if (!str) {
				return ba_ErrorMallocNoMem();
			}
			strcpy(str+oldLen, ctr->lex->val); // TODO: zero termination??
		}
		while (ba_PAccept(BA_TK_LITSTR, ctr));
		
		struct ba_Str* stkStr = malloc(sizeof(struct ba_Str));
		if (!stkStr) {
			return ba_ErrorMallocNoMem();
		}
		stkStr->str = str;
		stkStr->len = len;
		ba_PTkStkPush(ctr, (void*)stkStr, BA_TYPE_NONE, BA_TK_LITSTR);
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
		ba_PTkStkPush(ctr, (void*)num, type, BA_TK_LITINT);
	}
	// identifier
	else if (ba_PAccept(BA_TK_IDENTIFIER, ctr)) {
		struct ba_SymTable* stFoundIn = 0;
		struct ba_STVal* id = ba_STParentFind(ctr->currScope, &stFoundIn, lexVal);
		if (!id) {
			return ba_ErrorIdUndef(lexVal, lexLine, lexColStart);
		}
		ba_PTkStkPush(ctr, (void*)id, id->type, BA_TK_IDENTIFIER);
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
	switch (op->syntax) {
		case BA_OP_PREFIX:
			if (op->lexemeType == '+' || op->lexemeType == '-' || 
				op->lexemeType == '!' || op->lexemeType == '~')
			{
				return 1;
			}
			else if (op->lexemeType == '(') {
				return 254;
			}
			break;

		case BA_OP_INFIX:
			if (op->lexemeType == BA_TK_LSHIFT ||
				op->lexemeType == BA_TK_RSHIFT)
			{
				return 2;
			}
			else if (op->lexemeType == '*' || op->lexemeType == '%' || 
				op->lexemeType == BA_TK_DBSLASH)
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
			else if (op->lexemeType == BA_TK_DBAMPD) {
				return 8;
			}
			else if (op->lexemeType == BA_TK_DBBAR) {
				return 9;
			}
			break;

		case BA_OP_POSTFIX:
			if (op->lexemeType == ')') {
				return 255;
			}
			break;
	}
	
	printf("Error: unknown operator with lexeme type 0x%llx, "
		"syntax type %u\n", op->lexemeType, op->syntax);
	exit(-1);
	return 255;
}

// Handle operations (i.e. perform operation now or generate code for it)
u8 ba_POpHandle(struct ba_Controller* ctr) {
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

				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			else if (op->lexemeType == '-' || op->lexemeType == '~' ||
				op->lexemeType == '!')
			{
				if ((op->lexemeType == '~' && !ba_IsTypeIntegral(arg->type)) ||
					!ba_IsTypeNumeric(arg->type))
				{
					printf("Error: unary '%s' used with non %s operand on "
						"line %llu:%llu", ba_GetLexemeStr(op->lexemeType), 
						op->lexemeType == '~' ? "integral" : "numeric", 
						op->line, op->col);
					exit(-1);
				}

				u64 imOp;
				((op->lexemeType == '-') && (imOp = BA_IM_NEG)) ||
				((op->lexemeType == '~') && (imOp = BA_IM_NOT));

				if (ba_IsTypeIntegral(arg->type)) {
					u8 isArgLiteral = 
						arg->lexemeType != BA_TK_IDENTIFIER && 
						arg->lexemeType != BA_TK_IMRBPSUB && 
						arg->lexemeType != BA_TK_IMREGISTER;

					if (isArgLiteral) {
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

				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			else if (op->lexemeType == '(') {
				ba_StkPush(arg, ctr->pTkStk);
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
			
			// Bit shifts

			if (op->lexemeType == BA_TK_LSHIFT || 
				op->lexemeType == BA_TK_RSHIFT) 
			{
				if (!ba_IsTypeIntegral(lhs->type) || !ba_IsTypeIntegral(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift used with "
						"non integral operand(s) on", op->line, op->col);
				}

				arg->type = ba_IsTypeUnsigned(lhs->type) ? BA_TYPE_U64 : BA_TYPE_I64;
				
				u8 isLhsLiteral = 
					lhs->lexemeType != BA_TK_IDENTIFIER &&
					lhs->lexemeType != BA_TK_IMREGISTER && 
					lhs->lexemeType != BA_TK_IMRBPSUB;
				u8 isRhsLiteral = 
					rhs->lexemeType != BA_TK_IDENTIFIER &&
					rhs->lexemeType != BA_TK_IMREGISTER && 
					rhs->lexemeType != BA_TK_IMRBPSUB;

				// If rhs is 0, there is no change in in lhs
				if (isRhsLiteral && !rhs->val) {
					ba_StkPush(lhs, ctr->pTkStk);
					return 1;
				}

				if (isLhsLiteral && isRhsLiteral) {
					// If both are literals
					arg->val = (op->lexemeType == BA_TK_LSHIFT)
						? (void*)(((u64)lhs->val) << ((u64)rhs->val & 0x7f))
						: (void*)(((u64)lhs->val) >> ((u64)rhs->val & 0x7f));
				}
				else {
					// Handle non literal bit shifts
					u64 imOp = op->lexemeType == BA_TK_LSHIFT ? BA_IM_SHL : BA_IM_SAR;
					ba_POpNonLitBitShift(imOp, arg, lhs, rhs, isRhsLiteral, ctr);
				}

				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			
			// Multiplication, division, modulo
			
			else if (op->lexemeType == '*') {
				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "multiplication with "
						"non numeric operand(s) on", op->line, op->col);
				}

				if (ba_IsTypeIntegral(lhs->type) && ba_IsTypeIntegral(rhs->type)) {
					u64 argType = BA_TYPE_I64; // Default, most likely type
					if (ba_IsTypeUnsigned(lhs->type) ^ ba_IsTypeUnsigned(rhs->type)) {
						// Different signedness
						char* msgAfter = ba_IsWarningsAsErrors ? ""
							: ", implicitly converted lhs to i64";
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "multiplying integers of "
							"different signedness on", op->line, op->col, msgAfter);
					}
					else if (ba_IsTypeUnsigned(lhs->type)) {
						argType = BA_TYPE_U64;
					}
					arg->type = argType;
				}

				u8 isLhsLiteral = 
					lhs->lexemeType != BA_TK_IDENTIFIER &&
					lhs->lexemeType != BA_TK_IMREGISTER && 
					lhs->lexemeType != BA_TK_IMRBPSUB;
				u8 isRhsLiteral = 
					rhs->lexemeType != BA_TK_IDENTIFIER &&
					rhs->lexemeType != BA_TK_IMREGISTER && 
					rhs->lexemeType != BA_TK_IMRBPSUB;

				if (isLhsLiteral && isRhsLiteral) {
					arg->val = ba_IsTypeUnsigned(rhs->type)
						? (void*)(((u64)lhs->val) * ((u64)rhs->val))
						: (void*)(((i64)lhs->val) * ((i64)rhs->val));
				}
				else if (isLhsLiteral || isRhsLiteral) {
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
					// TODO: x * 2 -> x + x
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
						goto BA_LBL_POPHANDLE_MUL_NONLIT;
					}
				}
				else {
					BA_LBL_POPHANDLE_MUL_NONLIT:
					// TODO: make this a general infix function in parser_op.h
					
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
							ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
						}
						++ctr->imStackCnt;
						ctr->imStackSize += 8;
						lhsStackPos = ctr->imStackSize;
					}
					
					/* regR is replaced with rdx normally, but if regL is 
					 * already rdx, regR will be set to rcx. */
					u64 rhsReplacement = regL == BA_IM_RDX ? BA_IM_RCX : BA_IM_RDX;

					if (!regL) {
						// First: result location, second: preserve rax
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
					}

					if (!regR) {
						// Only once to preserve rdx/rcx
						ba_AddIM(&ctr->im, 2, BA_IM_PUSH, rhsReplacement);
					}

					if (lhs->lexemeType == BA_TK_IDENTIFIER) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX,
							BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, regL ? regL : BA_IM_RAX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
					}
					else if (isLhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX,
							BA_IM_IMM, (u64)lhs->val);
					}

					if (rhs->lexemeType == BA_TK_IDENTIFIER) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, 
							regR ? regR : rhsReplacement,
							BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
					}
					else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, 
							regR ? regR : rhsReplacement,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
					}
					else if (isRhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, 
							regR ? regR : rhsReplacement,
							BA_IM_IMM, (u64)rhs->val);
					}

					ba_AddIM(&ctr->im, 3, BA_IM_IMUL, regL ? regL : BA_IM_RAX, 
						regR ? regR : rhsReplacement);

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
					}
					else {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, rhsReplacement);
					}

					if (regL) {
						arg->lexemeType = BA_TK_IMREGISTER;
						arg->val = (void*)regL;
					}
					else {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
							lhsStackPos, BA_IM_RAX);
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);

						arg->lexemeType = BA_TK_IMRBPSUB;
						arg->val = (void*)ctr->imStackSize;
					}
				}

				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			else if (op->lexemeType == BA_TK_DBSLASH) {
				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "integer division with "
						"non numeric operand(s) on", op->line, op->col);
				}

				u64 argType = BA_TYPE_I64; // Default, most likely type
				u64 areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
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

				u8 isLhsLiteral = 
					lhs->lexemeType != BA_TK_IDENTIFIER &&
					lhs->lexemeType != BA_TK_IMREGISTER && 
					lhs->lexemeType != BA_TK_IMRBPSUB;
				u8 isRhsLiteral = 
					rhs->lexemeType != BA_TK_IDENTIFIER &&
					rhs->lexemeType != BA_TK_IMREGISTER && 
					rhs->lexemeType != BA_TK_IMRBPSUB;

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

					if (lhs->lexemeType == BA_TK_IDENTIFIER) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
					}
					else if (isLhsLiteral) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_IMM, (u64)lhs->val);
					}

					if (rhs->lexemeType == BA_TK_IDENTIFIER) {
						ba_AddIM(&ctr->im, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
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
						ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
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

				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			else if (op->lexemeType == '%') {
				if (ba_IsTypeUnsigned(lhs->type)) {
					if (ba_IsTypeUnsigned(rhs->type)) {
						arg->type = BA_TYPE_U64;
						arg->val = (void*)(((u64)lhs->val) % ((u64)rhs->val));
					}
					else if (ba_IsTypeSigned(rhs->type)) {
						char* msgAfter = "";
						if (!ba_IsWarningsAsErrors) {
							msgAfter = ", implicitly converted lhs to i64";
						}
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "modulo operation on integers "
							"of different signedness on", op->line, op->col, msgAfter);
						
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) % ((i64)rhs->val));
					}
					else {
						return ba_ExitMsg(BA_EXIT_ERR, "modulo operation with "
							"non numeric rhs operand on", op->line, op->col);
					}
				}
				else if (ba_IsTypeSigned(lhs->type)) {
					if (ba_IsTypeUnsigned(rhs->type)) {
						char* msgAfter = "";
						if (!ba_IsWarningsAsErrors) {
							msgAfter = ", implicitly converted rhs to i64";
						}
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "modulo operation on integers "
							"of different signedness on", op->line, op->col, msgAfter);
						
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) % ((i64)rhs->val));
					}
					else if (ba_IsTypeSigned(rhs->type)) {
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) % ((i64)rhs->val));
					}
					else {
						return ba_ExitMsg(BA_EXIT_ERR, "modulo operation with "
							"non numeric rhs operand on", op->line, op->col);
					}
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "modulo operation with "
						"non numeric lhs operand on", op->line, op->col);
				}
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			
			// Bitwise operations
			
			else if (op->lexemeType == '&' || op->lexemeType == '^' ||
				op->lexemeType == '|')
			{
				if (!ba_IsTypeIntegral(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift used with non integral "
						"rhs operand on", op->line, op->col);
				}
				
				if (ba_IsTypeUnsigned(lhs->type)) {
					arg->type = BA_TYPE_U64;
				}
				else if (ba_IsTypeSigned(lhs->type)) {
					arg->type = BA_TYPE_I64;
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift used with "
						"non integral lhs operand on", op->line, op->col);
				}

				if (op->lexemeType == '&') {
					arg->val = (void*)(((u64)lhs->val) & ((u64)rhs->val));
				}
				else if (op->lexemeType == '^') {
					arg->val = (void*)(((u64)lhs->val) ^ ((u64)rhs->val));
				}
				else if (op->lexemeType == '|') {
					arg->val = (void*)(((u64)lhs->val) | ((u64)rhs->val));
				}
				
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}

			// Addition, subtraction

			else if (op->lexemeType == '+') {
				if (ba_IsTypeUnsigned(lhs->type)) {
					if (ba_IsTypeUnsigned(rhs->type)) {
						arg->type = BA_TYPE_U64;
						arg->val = (void*)(((u64)lhs->val) + ((u64)rhs->val));
					}
					else if (ba_IsTypeSigned(rhs->type)) {
						char* msgAfter = "";
						if (!ba_IsWarningsAsErrors) {
							msgAfter = ", implicitly converted lhs to i64";
						}
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "adding integers "
							"of different signedness on", op->line, op->col, msgAfter);
						
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) + ((i64)rhs->val));
					}
					else {
						return ba_ExitMsg(BA_EXIT_ERR, "addition with "
							"non numeric rhs operand on", op->line, op->col);
					}
				}
				else if (ba_IsTypeSigned(lhs->type)) {
					if (ba_IsTypeUnsigned(rhs->type)) {
						char* msgAfter = "";
						if (!ba_IsWarningsAsErrors) {
							msgAfter = ", implicitly converted rhs to i64";
						}
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "adding integers of "
							"different signedness on", op->line, op->col, msgAfter);
						
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) + ((i64)rhs->val));
					}
					else if (ba_IsTypeSigned(rhs->type)) {
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) + ((i64)rhs->val));
					}
					else {
						return ba_ExitMsg(BA_EXIT_ERR, "addition with "
							"non numeric rhs operand on", op->line, op->col);
					}
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "addition with "
						"non numeric lhs operand on", op->line, op->col);
				}
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			else if (op->lexemeType == '-') {
				if (ba_IsTypeUnsigned(lhs->type)) {
					if (ba_IsTypeUnsigned(rhs->type)) {
						arg->type = BA_TYPE_U64;
						arg->val = (void*)(((u64)lhs->val) - ((u64)rhs->val));
					}
					else if (ba_IsTypeSigned(rhs->type)) {
						char* msgAfter = "";
						if (!ba_IsWarningsAsErrors) {
							msgAfter = ", implicitly converted lhs to i64";
						}
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "subtracting integers "
							"of different signedness on", op->line, op->col, msgAfter);
						
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) - ((i64)rhs->val));
					}
					else {
						return ba_ExitMsg(BA_EXIT_ERR, "subtraction with "
							"non numeric rhs operand on", op->line, op->col);
					}
				}
				else if (ba_IsTypeSigned(lhs->type)) {
					if (ba_IsTypeUnsigned(rhs->type)) {
						char* msgAfter = "";
						if (!ba_IsWarningsAsErrors) {
							msgAfter = ", implicitly converted rhs to i64";
						}
						ba_ExitMsg2(BA_EXIT_EXTRAWARN, "subtracting integers of "
							"different signedness on", op->line, op->col, msgAfter);
						
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) - ((i64)rhs->val));
					}
					else if (ba_IsTypeSigned(rhs->type)) {
						arg->type = BA_TYPE_I64;
						arg->val = (void*)(((i64)lhs->val) - ((i64)rhs->val));
					}
					else {
						return ba_ExitMsg(BA_EXIT_ERR, "subtraction with "
							"non numeric rhs operand on", op->line, op->col);
					}
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "subtraction with "
						"non numeric lhs operand on", op->line, op->col);
				}
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			
			// Logical operators
			// TODO: short circuiting
			else if (op->lexemeType == BA_TK_DBAMPD) {
				if (ba_IsTypeNumeric(lhs->type) && 
					ba_IsTypeNumeric(rhs->type))
				{
						arg->type = BA_TYPE_U8;
						arg->val = (void*)(u64)(lhs->val && rhs->val);
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "logical AND with "
						"non numeric operand(s) on", op->line, op->col);
				}
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			else if (op->lexemeType == BA_TK_DBBAR) {
				if (ba_IsTypeNumeric(lhs->type) || 
					ba_IsTypeNumeric(rhs->type))
				{
						arg->type = BA_TYPE_U8;
						arg->val = (void*)(u64)(lhs->val && rhs->val);
				}
				else {
					return ba_ExitMsg(BA_EXIT_ERR, "logical OR with "
						"non numeric operand(s) on", op->line, op->col);
				}
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}

			break;
		}

		case BA_OP_POSTFIX:
			// This should never occur
			if (op->lexemeType == ')') {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, op->col);
			}
			break;
	}
	
	return 1;
}

// Any type of expression
u8 ba_PExp(struct ba_Controller* ctr) {
	// Parse as if following an operator or parse as if following an atom?
	u8 afterAtom = 0;	

	// Counts grouping parentheses to make sure they are balanced
	i64 paren = 0;
	
	while (1) {
		u64 lexType = ctr->lex->type;
		u64 line = ctr->lex->line;
		u64 col = ctr->lex->colStart;
		
		// Start of an expression or after an operator
		if (!afterAtom) {
			// Prefix operator
			if (ba_PAccept('+', ctr) || ba_PAccept('-', ctr) || 
				ba_PAccept('!', ctr) || ba_PAccept('~', ctr) ||
				ba_PAccept('(', ctr))
			{
				// Left grouping parenthesis
				(lexType == '(') && ++paren;
				ba_POpStkPush(ctr, line, col, lexType, BA_OP_PREFIX);
			}
			// Atom: note that ba_PAtom pushes the atom to pTkStk
			else if (ba_PAtom(ctr)) {
				afterAtom = 1;
			}
			else {
				return 0;
			}
		}
		// After an atom, postfix or grouped expression
		else {
			struct ba_POpStkItem* op = 
				malloc(sizeof(struct ba_POpStkItem));
			if (!op) {
				return ba_ErrorMallocNoMem();
			}
			op->line = line;
			op->col = col;
			op->lexemeType = lexType;

			// Postfix operator
			if (ba_PAccept(')', ctr)) {
				// Right grouping parenthesis
				if (lexType == ')') {
					--paren;
				}
				
				op->syntax = BA_OP_POSTFIX;

				if (ctr->pOpStk->count) {
					do {
						if (ba_POpPrecedence(ba_StkTop(ctr->pOpStk)) <=
							ba_POpPrecedence(op))
						{
							u8 handle = ba_POpHandle(ctr);
							if (!handle) {
								return 0;
							}
							// Left grouping parenthesis
							else if (handle == 2) {
								goto BA_LBL_PEXP_LOOPEND;
							}
						}
						else {
							break;
						}
					}
					while (ctr->pOpStk->count);
				}

				ba_StkPush(op, ctr->pOpStk);
			}
			// Infix operator
			else if (ba_PAccept(BA_TK_LSHIFT, ctr) || 
				ba_PAccept(BA_TK_RSHIFT, ctr) || ba_PAccept('*', ctr) ||
				ba_PAccept(BA_TK_DBSLASH, ctr) || ba_PAccept('%', ctr) ||
				ba_PAccept('&', ctr) || ba_PAccept('^', ctr) || 
				ba_PAccept('|', ctr) || ba_PAccept('+', ctr) || 
				ba_PAccept('-', ctr) || ba_PAccept(BA_TK_DBAMPD, ctr) ||
				ba_PAccept(BA_TK_DBBAR, ctr))
			{
				op->syntax = BA_OP_INFIX;

				if (ctr->pOpStk->count) {
					do {
						if (ba_POpPrecedence(ba_StkTop(ctr->pOpStk)) <=
							ba_POpPrecedence(op))
						{
							u8 handle = ba_POpHandle(ctr);
							if (!handle) {
								return 0;
							}
							// Left grouping parenthesis
							else if (handle == 2) {
								goto BA_LBL_PEXP_LOOPEND;
							}
						}
						else {
							break;
						}
					}
					while (ctr->pOpStk->count);
				}

				ba_StkPush(op, ctr->pOpStk);
				afterAtom = 0;
			}
			// Other tokens
			else {
				goto BA_LBL_PEXP_END;
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
		if (!ba_POpHandle(ctr)) {
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
	ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, memLen);
}

/* stmt = "write" exp ";" 
 *      | base_type identifier [ "=" exp ] ";" 
 *      | exp ";"
 *      | ";" 
 */
u8 ba_PStmt(struct ba_Controller* ctr) {
	// TODO: this should eventually be replaced with a Write() function
	// and a lone string literal statement
	
	// "write" ...
	if (ba_PAccept(BA_TK_KW_WRITE, ctr)) {
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
			if (stkItem->lexemeType == BA_TK_IDENTIFIER ||
				stkItem->lexemeType == BA_TK_IMREGISTER)
			{
				// U64ToStr is needed
				if (!ba_BltinFlagsTest(BA_BLTIN_U64ToStr)) {
					ba_BltinU64ToStr(ctr);
				}

				if (stkItem->lexemeType == BA_TK_IDENTIFIER) {
					// Load the data and call U64ToStr
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, 
						BA_IM_DATASGMT, ((struct ba_STVal*)stkItem->val)->address);
					ba_AddIM(&ctr->im, 2, BA_IM_LABELCALL, 
						ba_BltinLabels[BA_BLTIN_U64ToStr]);
				}
				else if (stkItem->lexemeType == BA_TK_IMREGISTER) {
					if ((u64)stkItem->val != BA_IM_RAX) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, 
							(u64)stkItem->val);
					}
					ba_AddIM(&ctr->im, 2, BA_IM_LABELCALL, 
						ba_BltinLabels[BA_BLTIN_U64ToStr]);
				}

				// write
				ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RAX);
				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RDI, BA_IM_IMM, 1);
				ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RSI, BA_IM_RSP);
				ba_AddIM(&ctr->im, 1, BA_IM_SYSCALL);

				// deallocate stack memory
				ba_AddIM(&ctr->im, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 0x18);

				// ... ';'
				ba_PExpect(';', ctr);

				return 1;
			}
			else {
				str = ba_U64ToStr((u64)stkItem->val);
				len = strlen(str);
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

		idVal = malloc(sizeof(struct ba_STVal)); //TODO: free later on
		if (!idVal) {
			return ba_ErrorMallocNoMem();
		}
		idVal->parent = ctr->currScope;
		switch (varTypeItem->lexemeType) {
			case BA_TK_KW_U64:
				idVal->type = BA_TYPE_U64;
				break;
			case BA_TK_KW_I64:
				idVal->type = BA_TYPE_I64;
				break;
		}
		
		idVal->address = ctr->dataSgmtSize;
		ctr->dataSgmtSize += ba_GetSizeOfType(idVal->type);
		
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

				if (expItem->lexemeType == BA_TK_IDENTIFIER) {
					// Load the data into RAX, then load RAX into id address
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT, 
						((struct ba_STVal*)expItem->val)->address);
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						idVal->address, BA_IM_RAX);
				}
				else if (expItem->lexemeType == BA_TK_IMREGISTER) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						idVal->address, (u64)expItem->val);
				}
				else {
					idVal->initVal = expItem->val;
				}
			}
		}

		if (!ba_PExpect(';', ctr)) {
			return 0;
		}

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
			return 1;
		}

		// TODO
		
		if (!ba_PExpect(';', ctr)) {
			return 0;
		}

		return 1;
	}
	// ";"
	else if (ba_PAccept(';', ctr)) {
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
