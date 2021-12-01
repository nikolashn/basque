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
		ba_PTkStkPush(ctr, /* val = */ 0, BA_TYPE_TYPE, BA_TK_KW_U64, 
			/* isLValue = */ 0);
	}
	else if (ba_PAccept(BA_TK_KW_I64, ctr)) {
		ba_PTkStkPush(ctr, /* val = */ 0, BA_TYPE_TYPE, BA_TK_KW_I64, 
			/*isLValue = */ 0);
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
		ba_PTkStkPush(ctr, (void*)stkStr, BA_TYPE_NONE, BA_TK_LITSTR, 
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
		ba_PTkStkPush(ctr, (void*)num, type, BA_TK_LITINT, 
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
		ba_PTkStkPush(ctr, (void*)id, id->type, BA_TK_IDENTIFIER, 
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
			else if (op->lexemeType == BA_TK_LOGAND) {
				return 8;
			}
			else if (op->lexemeType == BA_TK_LOGOR) {
				return 9;
			}
			else if (op->lexemeType == '=' || 
				ba_IsLexemeCompoundAssign(op->lexemeType))
			{
				return 11;
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
				
				arg->isLValue = 0;
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

				arg->isLValue = 0;
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
			
			u8 isLhsLiteral = 
				lhs->lexemeType != BA_TK_IDENTIFIER &&
				lhs->lexemeType != BA_TK_IMREGISTER && 
				lhs->lexemeType != BA_TK_IMRBPSUB;
			u8 isRhsLiteral = 
				rhs->lexemeType != BA_TK_IDENTIFIER &&
				rhs->lexemeType != BA_TK_IMREGISTER && 
				rhs->lexemeType != BA_TK_IMRBPSUB;

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
					ba_StkPush(lhs, ctr->pTkStk);
					return 1;
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
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
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
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}

			// Division

			else if (op->lexemeType == BA_TK_IDIV) {
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
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}

			// Modulo

			else if (op->lexemeType == '%') {
				/* Modulo in Basque is from floored divison (like in Python) */

				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "modulo with "
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
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
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
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
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
				ba_StkPush(arg, ctr->pTkStk);
				return 1;
			}
			
			// Assignment
			else if (op->lexemeType == '=' || 
				ba_IsLexemeCompoundAssign(op->lexemeType)) 
			{
				// TODO: change the 2nd condition when other lvalues are added
				if (!lhs->isLValue || lhs->lexemeType != BA_TK_IDENTIFIER) {
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

				if (rhs->lexemeType == BA_TK_IDENTIFIER) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
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

					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT,
						((struct ba_STVal*)lhs->val)->address);
					ba_AddIM(&ctr->im, 1, BA_IM_CQO);
					ba_AddIM(&ctr->im, 2, areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV, 
						reg ? reg : BA_IM_RCX);
					
					u64 divResultReg = BA_TK_IDIVEQ ? BA_IM_RAX : BA_IM_RDX;
					if (reg != divResultReg) {
						ba_AddIM(&ctr->im, 3, BA_IM_MOV, reg ? reg : BA_IM_RCX,
							divResultReg);
					}
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						((struct ba_STVal*)lhs->val)->address, 
						reg ? reg : BA_IM_RCX);

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

					ba_AddIM(&ctr->im, 4, BA_IM_MOV, opResultReg, BA_IM_DATASGMT, 
						((struct ba_STVal*)lhs->val)->address);

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
						ba_AddIM(&ctr->im, 3, imOp, opResultReg, reg ? reg : BA_IM_RCX);
					}

					ba_AddIM(&ctr->im, 3, BA_IM_MOV, reg ? reg : BA_IM_RCX, 
						opResultReg);

					if (isPushOpResReg) {
						ba_AddIM(&ctr->im, 2, BA_IM_POP, opResultReg);
					}
				}

				ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_DATASGMT, 
					((struct ba_STVal*)lhs->val)->address, 
					reg ? reg : BA_IM_RCX);

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
			struct ba_POpStkItem* op = malloc(sizeof(*op));
			if (!op) {
				return ba_ErrorMallocNoMem();
			}

			op->line = line;
			op->col = col;
			op->lexemeType = lexType;
			op->syntax = 0;

			// Set syntax type
			if (ba_PAccept(')', ctr)) {
				op->syntax = BA_OP_POSTFIX;
			}
			else if (ba_PAccept(BA_TK_LSHIFT, ctr) || 
				ba_PAccept(BA_TK_RSHIFT, ctr) || ba_PAccept('*', ctr) || 
				ba_PAccept(BA_TK_IDIV, ctr) || ba_PAccept('%', ctr) || 
				ba_PAccept('&', ctr) || ba_PAccept('^', ctr) || 
				ba_PAccept('|', ctr) || ba_PAccept('+', ctr) || 
				ba_PAccept('-', ctr) || ba_PAccept(BA_TK_LOGAND, ctr) || 
				ba_PAccept(BA_TK_LOGOR, ctr) || ba_PAccept('=', ctr) ||
				ba_PAccept(BA_TK_ADDEQ, ctr) || ba_PAccept(BA_TK_SUBEQ, ctr) || 
				ba_PAccept(BA_TK_MULEQ, ctr) || ba_PAccept(BA_TK_IDIVEQ, ctr) || 
				ba_PAccept(BA_TK_FDIVEQ, ctr) || ba_PAccept(BA_TK_MODEQ, ctr) || 
				ba_PAccept(BA_TK_LSHIFTEQ, ctr) || ba_PAccept(BA_TK_RSHIFTEQ, ctr) || 
				ba_PAccept(BA_TK_BITANDEQ, ctr) || ba_PAccept(BA_TK_BITXOREQ, ctr) || 
				ba_PAccept(BA_TK_BITOREQ, ctr))
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

			// Short circuiting: jmp ahead if right-hand side operand is unnecessary
			if (lexType == BA_TK_LOGAND || lexType == BA_TK_LOGOR) {
				struct ba_PTkStkItem* lhs = ba_StkTop(ctr->pTkStk);
				ba_StkPush((void*)ctr->labelCnt, ctr->shortCircLblStk);

				u64 reg = (u64)lhs->val; // Kept only if lhs is a register
				
				if (lhs->lexemeType != BA_TK_IMREGISTER) {
					reg = ba_NextIMRegister(ctr);
				}

				if (!reg) { // Preserve rax
					ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
				}
				
				u64 realReg = reg ? reg : BA_IM_RAX;
				if (lhs->lexemeType == BA_TK_IDENTIFIER) {
					ba_AddIM(&ctr->im, 4, BA_IM_MOV, realReg, BA_IM_DATASGMT,
						((struct ba_STVal*)lhs->val)->address);
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

				if (reg && lhs->lexemeType != BA_TK_IMREGISTER) {
					ctr->usedRegisters &= ~ba_IMToCtrRegister(reg);
				}

				++ctr->labelCnt;
			}

			ba_StkPush(op, ctr->pOpStk);

			if (op->syntax == BA_OP_INFIX) {
				afterAtom = 0;
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
