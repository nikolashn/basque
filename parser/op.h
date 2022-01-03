// See LICENSE for copyright/license information

#ifndef BA__PARSER_OP_H
#define BA__PARSER_OP_H

#include "../lexer.h"
#include "../bltin/bltin.h"

// Some operators cannot handle other operators
bool ba_POpIsHandler(struct ba_POpStkItem* op) {
	return !(op->syntax == BA_OP_POSTFIX && op->lexemeType == '(');
}

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
			else if (op->lexemeType == '(') {
				return 99;
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

/* NOTE: The following functions are intended to be called by multiple 
 * different operators and so should not contain any (source code) operator 
 * specific exit messages. */

// Handle unary operators with a non literal operand
void ba_POpNonLitUnary(u64 opLexType, struct ba_PTkStkItem* arg,
	struct ba_Controller* ctr)
{
	u64 imOp = 0;
	((opLexType == '-') && (imOp = BA_IM_NEG)) ||
	((opLexType == '~') && (imOp = BA_IM_NOT)) ||
	((opLexType == '!') && (imOp = 1));

	if (!imOp) {
		fprintf(stderr, "Error: Unary lexeme type %llx passed to "
			"ba_POpNonLitUnary not recognized\n", opLexType);
		exit(-1);
	}

	u64 stackPos = 0;
	u64 reg = (u64)arg->val; // Kept only if arg is a register

	if (arg->lexemeType != BA_TK_IMREGISTER) {
		reg = ba_NextIMRegister(ctr);
	}

	if (!reg) {
		if (!ctr->imStackSize) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, 
				BA_IM_RSP);
		}
		stackPos = ctr->imStackSize + 8;
		// First: result location, second: preserve rax
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ctr->imStackSize += 16;
	}

	u64 realReg = reg ? reg : BA_IM_RAX;

	if (arg->lexemeType == BA_TK_GLOBALID) {
		ba_AddIM(ctr, 4, BA_IM_MOV, realReg, 
			BA_IM_DATASGMT, 
			((struct ba_STVal*)arg->val)->address);
	}
	else if (arg->lexemeType == BA_TK_LOCALID) {
		ba_AddIM(ctr, 5, BA_IM_MOV, realReg, 
			BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
			ba_CalcSTValOffset(ctr->currScope, arg->val));
	}
	else if (arg->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, BA_IM_MOV, realReg, 
			BA_IM_ADRSUB, BA_IM_RBP, (u64)arg->val);
	}

	if (opLexType == '!') {
		ba_AddIM(ctr, 3, BA_IM_TEST, realReg, realReg);
		ba_AddIM(ctr, 2, BA_IM_SETZ, 
			realReg - BA_IM_RAX + BA_IM_AL);
		ba_AddIM(ctr, 3, BA_IM_MOVZX, realReg, 
			realReg - BA_IM_RAX + BA_IM_AL);
	}
	else {
		ba_AddIM(ctr, 2, imOp, realReg);
	}

	if (reg) {
		arg->lexemeType = BA_TK_IMREGISTER;
		arg->val = (void*)reg;
	}
	else {
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, 
			BA_IM_RBP, stackPos, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;

		arg->lexemeType = BA_TK_IMRBPSUB;
		arg->val = (void*)ctr->imStackSize;
	}
}

// Handle binary operators with a non literal operand
void ba_POpNonLitBinary(u64 imOp, struct ba_PTkStkItem* arg,
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	bool isLhsLiteral, bool isRhsLiteral, bool isShortCirc, 
	struct ba_Controller* ctr)
{
	u64 lhsStackPos = 0;
	u64 regL = (u64)lhs->val; // Kept only if lhs is a register
	u64 regR = (u64)rhs->val; // Kept only if rhs is a register

	// Return 0 means on the stack
	(lhs->lexemeType != BA_TK_IMREGISTER) &&
		(regL = ba_NextIMRegister(ctr));
	(rhs->lexemeType != BA_TK_IMREGISTER) &&
		(regR = ba_NextIMRegister(ctr));

	if (!regL) {
		if (!ctr->imStackSize) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
		}
		lhsStackPos = ctr->imStackSize + 8;
		// First: result location, second: preserve rax
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ctr->imStackSize += 16;
	}
	
	/* regR is replaced with rdx normally, but if regL is 
	 * already rdx, regR will be set to rcx. */
	u64 rhsReplacement = regL == BA_IM_RDX ? BA_IM_RCX : BA_IM_RDX;

	if (!regR) {
		// Only once to preserve rdx/rcx
		ba_AddIM(ctr, 2, BA_IM_PUSH, rhsReplacement);
		ctr->imStackSize += 8;
	}

	if (lhs->lexemeType == BA_TK_GLOBALID) {
		ba_AddIM(ctr, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX,
			BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
	}
	else if (lhs->lexemeType == BA_TK_LOCALID) {
		ba_AddIM(ctr, 5, BA_IM_MOV, regL ? regL : BA_IM_RAX,
			BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
			ba_CalcSTValOffset(ctr->currScope, lhs->val));
	}
	else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, BA_IM_MOV, regL ? regL : BA_IM_RAX,
			BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
	}
	else if (isLhsLiteral) {
		ba_AddIM(ctr, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX,
			BA_IM_IMM, (u64)lhs->val);
	}

	if (rhs->lexemeType == BA_TK_GLOBALID) {
		ba_AddIM(ctr, 4, BA_IM_MOV, regR ? regR : rhsReplacement,
			BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
	}
	else if (rhs->lexemeType == BA_TK_LOCALID) {
		ba_AddIM(ctr, 5, BA_IM_MOV, regR ? regR : rhsReplacement,
			BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
			ba_CalcSTValOffset(ctr->currScope, rhs->val));
	}
	else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, BA_IM_MOV, 
			regR ? regR : rhsReplacement,
			BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
	}
	else if (isRhsLiteral) {
		ba_AddIM(ctr, 4, BA_IM_MOV, 
			regR ? regR : rhsReplacement,
			BA_IM_IMM, (u64)rhs->val);
	}

	ba_AddIM(ctr, 3, imOp, regL ? regL : BA_IM_RAX, 
		regR ? regR : rhsReplacement);

	if (isShortCirc) {
		u64 realRegL = regL ? regL : BA_IM_RAX;
		ba_AddIM(ctr, 2, BA_IM_LABEL, ba_StkPop(ctr->shortCircLblStk));
		ba_AddIM(ctr, 2, BA_IM_SETNZ, realRegL - BA_IM_RAX + BA_IM_AL);
		ba_AddIM(ctr, 3, BA_IM_MOVZX, realRegL,
			realRegL - BA_IM_RAX + BA_IM_AL);
	}

	if (regR) {
		ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
	}
	else {
		ba_AddIM(ctr, 2, BA_IM_POP, rhsReplacement);
		ctr->imStackSize -= 8;
	}

	if (regL) {
		arg->lexemeType = BA_TK_IMREGISTER;
		arg->val = (void*)regL;
	}
	else {
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
			lhsStackPos, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;

		arg->lexemeType = BA_TK_IMRBPSUB;
		arg->val = (void*)ctr->imStackSize;
	}
}

// Handle bit shifts with at least one non literal operand
void ba_POpNonLitBitShift(u64 imOp, struct ba_PTkStkItem* arg,
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	bool isRhsLiteral, struct ba_Controller* ctr) 
{
	bool isLhsOriginallyRcx = 0;
	u64 lhsStackPos = 0;
	u64 regL = (u64)lhs->val; // Kept only if lhs is a register

	// Return 0 means on the stack
	(lhs->lexemeType != BA_TK_IMREGISTER) &&
		(regL = ba_NextIMRegister(ctr));

	if (regL == BA_IM_RCX) {
		(lhs->lexemeType == BA_TK_IMREGISTER) &&
			(isLhsOriginallyRcx = 1);
		regL = ba_NextIMRegister(ctr);
		ctr->usedRegisters &= ~BA_CTRREG_RCX;
	}

	if (!regL) {
		if (!ctr->imStackSize) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
		}
		lhsStackPos = ctr->imStackSize + 8;
		// First: result location, second: preserve rax
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ctr->imStackSize += 16;
	}

	if (lhs->lexemeType == BA_TK_GLOBALID) {
		ba_AddIM(ctr, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX, 
			BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
	}
	else if (lhs->lexemeType == BA_TK_LOCALID) {
		ba_AddIM(ctr, 5, BA_IM_MOV, regL ? regL : BA_IM_RAX, 
			BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
			ba_CalcSTValOffset(ctr->currScope, lhs->val));
	}
	else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, BA_IM_MOV, regL ? regL : BA_IM_RAX,
			BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
	}
	else if (lhs->lexemeType == BA_TK_IMREGISTER) {
		if (isLhsOriginallyRcx) {
			ba_AddIM(ctr, 3, BA_IM_MOV, 
				regL ? regL : BA_IM_RAX, BA_IM_RCX);
		}
	}
	else { // Literal (immediate) lhs
		ba_AddIM(ctr, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX, 
			BA_IM_IMM, (u64)lhs->val);
	}

	if (isRhsLiteral) {
		ba_AddIM(ctr, 4, imOp, regL ? regL : BA_IM_RAX, 
			BA_IM_IMM, (u64)rhs->val & 0x3f);
	}
	else {
		u64 regTmp = BA_IM_RCX;
		bool rhsIsRcx = rhs->lexemeType == BA_TK_IMREGISTER &&
			(u64)rhs->val == BA_IM_RCX;

		if ((ctr->usedRegisters & BA_CTRREG_RCX) && !rhsIsRcx) {
			regTmp = ba_NextIMRegister(ctr);
			if (!regTmp) {
				// Don't need to store rhs, only preserve rax
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
				ctr->imStackSize += 8;
			}
			ba_AddIM(ctr, 3, BA_IM_MOV, 
				regTmp ? regTmp : BA_IM_RAX, BA_IM_RCX);
		}

		if (rhs->lexemeType == BA_TK_GLOBALID) {
			ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RCX, BA_IM_DATASGMT,
				((struct ba_STVal*)rhs->val)->address);
		}
		else if (rhs->lexemeType == BA_TK_LOCALID) {
			ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RCX, 
				BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
				ba_CalcSTValOffset(ctr->currScope, rhs->val));
		}
		else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
			ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RCX,
				BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
		}
		else if (!rhsIsRcx) { // Register that isn't rcx
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, 
				(u64)rhs->val);
			ctr->usedRegisters &= ~ba_IMToCtrRegister((u64)rhs->val);
		}

		// The actual shift operation
		ba_AddIM(ctr, 3, imOp, regL ? regL : BA_IM_RAX, 
			BA_IM_CL);

		if ((ctr->usedRegisters & BA_CTRREG_RCX) && !rhsIsRcx) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, 
				regTmp ? regTmp : BA_IM_RAX);
		}

		if (!regTmp) {
			ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
			ctr->imStackSize -= 8;
		}

		if (regTmp && regTmp != BA_IM_RCX) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, regTmp);
			ctr->usedRegisters &= ~ba_IMToCtrRegister(regTmp);
		}
	}

	if (regL) {
		arg->lexemeType = BA_TK_IMREGISTER;
		arg->val = (void*)regL;
	}
	else {
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
			lhsStackPos, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;
		arg->lexemeType = BA_TK_IMRBPSUB;
		arg->val = (void*)ctr->imStackSize;
	}
}

// Right associative operators don't handle operators of the same precedence
u8 ba_POpIsRightAssoc(struct ba_POpStkItem* op) {
	if (op->syntax == BA_OP_INFIX) {
		return op->lexemeType == '=' || ba_IsLexemeCompoundAssign(op->lexemeType);
	}
	else if (op->syntax == BA_OP_PREFIX || op->syntax == BA_OP_POSTFIX) {
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
				return 1;
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
				return 1;
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
					arg->type = BA_TYPE_BOOL;
				}
				else if (ba_IsTypeUnsigned(arg->type)) {
					arg->type = BA_TYPE_U64;
				}
				else if (ba_IsTypeSigned(arg->type)) {
					arg->type = BA_TYPE_I64;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
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
					return ba_ExitMsg(BA_EXIT_ERR, "increment of non-numeric "
						"lvalue on", op->line, op->col);
				}

				u64 imOp = op->lexemeType == BA_TK_INC ? BA_IM_INC : BA_IM_DEC;
				u64 stackPos = 0;
				u64 reg = ba_NextIMRegister(ctr);
				
				if (!reg) {
					if (!ctr->imStackSize) {
						ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
					}
					stackPos = ctr->imStackSize + 8;
					// First: result location, second: preserve rcx
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
					ctr->imStackSize += 16;
				}

				if (arg->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(ctr, 4, BA_IM_MOV, reg ? reg : BA_IM_RAX,
						BA_IM_DATASGMT, ((struct ba_STVal*)arg->val)->address);
					ba_AddIM(ctr, 2, imOp, reg ? reg : BA_IM_RAX);
					ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						((struct ba_STVal*)arg->val)->address, 
						reg ? reg : BA_IM_RAX);
				}
				else {
					u64 ofst = ba_CalcSTValOffset(ctr->currScope, arg->val);
					ba_AddIM(ctr, 5, BA_IM_MOV, reg ? reg : BA_IM_RAX,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, ofst);
					ba_AddIM(ctr, 2, imOp, reg ? reg : BA_IM_RAX);
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, 
						ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ofst, reg ? reg : BA_IM_RAX);
				}

				if (reg) {
					arg->lexemeType = BA_TK_IMREGISTER;
					arg->val = (void*)reg;
				}
				else {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
						stackPos, BA_IM_RAX);
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
					ctr->imStackSize -= 16;

					arg->lexemeType = BA_TK_IMRBPSUB;
					arg->val = (void*)ctr->imStackSize;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
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
			
			bool isLhsLiteral = ba_IsLexemeLiteral(lhs->lexemeType);
			bool isRhsLiteral = ba_IsLexemeLiteral(rhs->lexemeType);

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
				ba_StkPush(ctr->pTkStk, arg);
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
				// TODO: add and sub optimization to inc and dec
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
				return 1;
			}

			// Division

			else if (op->lexemeType == BA_TK_IDIV) {
				if (!ba_IsTypeNumeric(lhs->type) || !ba_IsTypeNumeric(rhs->type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "integer division with "
						"non numeric operand(s) on", op->line, op->col);
				}

				u64 argType = BA_TYPE_I64; // Default, most likely type
				bool areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
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

					bool isRhsNeg = ba_IsTypeSigned(rhs->type) && (i64)rhs->val < 0;

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
						if (!ctr->imStackSize) {
							ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
						}
						lhsStackPos = ctr->imStackSize + 8;
						// Result location
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
						ctr->imStackSize += 8;
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

					bool isRaxPushed = 0;
					bool isRdxPushed = 0;

					if ((ctr->usedRegisters & BA_CTRREG_RAX) && (regL != BA_IM_RAX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(ctr, 3, BA_IM_MOV, regR, BA_IM_RAX);
						}
						else {
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ctr->imStackSize += 8;
							isRaxPushed = 1;
						}
					}

					if ((ctr->usedRegisters & BA_CTRREG_RDX) && (regL != BA_IM_RDX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(ctr, 3, BA_IM_MOV, regR, BA_IM_RDX);
						}
						else {
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDX);
							ctr->imStackSize += 8;
							isRdxPushed = 1;
						}
					}

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
					}
					else {
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
						ctr->imStackSize += 8;
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
					}
					else if (lhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, lhs->val));
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRSUB, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
							(u64)lhs->val);
					}
					else if (lhs->lexemeType == BA_TK_IMREGISTER && 
						(u64)lhs->val != BA_IM_RAX) 
					{
						ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, 
							(u64)lhs->val);
					}
					else if (isLhsLiteral) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_IMM, (u64)lhs->val);
					}

					if (rhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
					}
					else if (rhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, rhs->val));
					}
					else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(ctr, 5, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
					}
					else if (isRhsLiteral) {
						ba_AddIM(ctr, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_IMM, (u64)rhs->val);
					}

					ba_AddIM(ctr, 1, BA_IM_CQO);

					u64 imOp = areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV;
					ba_AddIM(ctr, 2, imOp, regR ? regR : BA_IM_RCX);

					if (regL) {
						if (regL != BA_IM_RAX) {
							ba_AddIM(ctr, 3, BA_IM_MOV, regL, BA_IM_RAX);
						}
						arg->lexemeType = BA_TK_IMREGISTER;
						arg->val = (void*)regL;
					}
					else {
						ba_AddIM(ctr, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
							lhsStackPos, BA_IM_RAX);
						arg->lexemeType = BA_TK_IMRBPSUB;
						arg->val = (void*)ctr->imStackSize;
					}

					if (isRdxPushed) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
						ctr->imStackSize -= 8;
					}

					if (lhs->lexemeType == BA_TK_IMREGISTER) {
						ctr->usedRegisters &= ~BA_CTRREG_RDX;
					}

					if (!regR) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
						ctr->imStackSize -= 8;
					}

					if (isRaxPushed) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						ctr->imStackSize -= 8;
					}
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
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
				bool areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
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

					bool isRhsNeg = ba_IsTypeSigned(rhs->type) && 
						(i64)rhs->val < 0;
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
						if (!ctr->imStackSize) {
							ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
						}
						lhsStackPos = ctr->imStackSize + 8;
						// Result location
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
						ctr->imStackSize += 8;
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

					bool isRaxPushed = 0;
					bool isRdxPushed = 0;

					if ((ctr->usedRegisters & BA_CTRREG_RAX) && (regL != BA_IM_RAX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(ctr, 3, BA_IM_MOV, regR, BA_IM_RAX);
						}
						else {
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ctr->imStackSize += 8;
							isRaxPushed = 1;
						}
					}

					if ((ctr->usedRegisters & BA_CTRREG_RDX) && (regL != BA_IM_RDX)) {
						if (rhs->lexemeType == BA_TK_IMREGISTER && 
							(u64)rhs->val == BA_IM_RAX) 
						{
							ba_AddIM(ctr, 3, BA_IM_MOV, regR, BA_IM_RDX);
						}
						else {
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDX);
							ctr->imStackSize += 8;
							isRdxPushed = 1;
						}
					}

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrRegister(regR);
					}
					else {
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
						ctr->imStackSize += 8;
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
					}
					else if (lhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, lhs->val));
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
					}
					else if (isLhsLiteral) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX,
							BA_IM_IMM, (u64)lhs->val);
					}

					if (rhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
					}
					else if (rhs->lexemeType == BA_TK_LOCALID) {
						ba_AddIM(ctr, 5, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
							ba_CalcSTValOffset(ctr->currScope, rhs->val));
					}
					else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
						ba_AddIM(ctr, 5, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
					}
					else if (isRhsLiteral) {
						ba_AddIM(ctr, 4, BA_IM_MOV, regR ? regR : BA_IM_RCX,
							BA_IM_IMM, (u64)rhs->val);
					}

					ba_AddIM(ctr, 1, BA_IM_CQO);

					u64 imOp = areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV;
					ba_AddIM(ctr, 2, imOp, regR ? regR : BA_IM_RCX);

					if (!areBothUnsigned) {
						// Test result for sign: if negative, then they had opposite
						// signs and so the rhs needs to be added
						
						// Sign stored in rax
						ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RAX, BA_IM_RAX);
						ba_AddIM(ctr, 2, BA_IM_SETS, BA_IM_AL);
						ba_AddIM(ctr, 3, BA_IM_MOVZX, BA_IM_RAX, BA_IM_AL);

						ba_AddIM(ctr, 3, BA_IM_IMUL, BA_IM_RAX, 
							regR ? regR : BA_IM_RCX);
						ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RDX, BA_IM_RAX);
					}

					if (regL) {
						if (regL != BA_IM_RDX) {
							ba_AddIM(ctr, 3, BA_IM_MOV, regL, BA_IM_RDX);
						}
						arg->lexemeType = BA_TK_IMREGISTER;
						arg->val = (void*)regL;
					}
					else {
						ba_AddIM(ctr, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
							lhsStackPos, BA_IM_RDX);
						arg->lexemeType = BA_TK_IMRBPSUB;
						arg->val = (void*)ctr->imStackSize;
					}

					if (isRdxPushed) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
						ctr->imStackSize -= 8;
					}

					if (lhs->lexemeType == BA_TK_IMREGISTER) {
						ctr->usedRegisters &= ~BA_CTRREG_RDX;
					}

					if (!regR) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
						ctr->imStackSize -= 8;
					}

					if (isRaxPushed) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						ctr->imStackSize -= 8;
					}
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
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
				ba_StkPush(ctr->pTkStk, arg);
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

				arg->type = BA_TYPE_BOOL;

				u64 imOp = op->lexemeType == BA_TK_LOGAND
					? BA_IM_AND : BA_IM_OR;

				// Although it is called "NonLit", it does work with literals 
				// as well. The reason for the name is that it isn't generally 
				// used for that.
				ba_POpNonLitBinary(imOp, arg, lhs, rhs, isLhsLiteral, 
					isRhsLiteral, /* isShortCirc = */ 1, ctr);
				
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
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
					if (!ctr->imStackSize) {
						ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
					}
					stackPos = ctr->imStackSize + 8;
					// First: result location, second: preserve rcx
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
					ctr->imStackSize += 16;
				}

				if (rhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(ctr, 4, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
				}
				else if (rhs->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(ctr, 5, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, rhs->val));
				}
				else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(ctr, 5, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
				}
				else if (rhs->lexemeType != BA_TK_IMREGISTER) {
					ba_AddIM(ctr, 4, BA_IM_MOV, reg ? reg : BA_IM_RCX,
						BA_IM_IMM, (u64)rhs->val);
				}

				bool areBothUnsigned = ba_IsTypeUnsigned(lhs->type) &&
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
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
						ctr->imStackSize += 8;
					}
					if (ctr->usedRegisters & BA_CTRREG_RDX) {
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDX);
						ctr->imStackSize += 8;
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_DATASGMT,
							((struct ba_STVal*)lhs->val)->address);
					}
					else {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, 
							BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
							lhsOffset);
					}

					ba_AddIM(ctr, 1, BA_IM_CQO);
					ba_AddIM(ctr, 2, areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV, 
						reg ? reg : BA_IM_RCX);
					
					u64 divResultReg = BA_TK_IDIVEQ ? BA_IM_RAX : BA_IM_RDX;
					if (reg != divResultReg) {
						ba_AddIM(ctr, 3, BA_IM_MOV, reg ? reg : BA_IM_RCX,
							divResultReg);
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_DATASGMT, 
							((struct ba_STVal*)lhs->val)->address, 
							reg ? reg : BA_IM_RCX);
					}
					else {
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, 
							BA_IM_RBP, lhsOffset, reg ? reg : BA_IM_RCX);
					}

					if (ctr->usedRegisters & BA_CTRREG_RDX) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
						ctr->imStackSize -= 8;
					}
					if (ctr->usedRegisters & BA_CTRREG_RAX) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						ctr->imStackSize -= 8;
					}
				}
				else if (imOp) {
					u64 opResultReg = reg == BA_IM_RAX ? BA_IM_RDX : BA_IM_RAX;
					bool isPushOpResReg = 
						((reg == BA_IM_RAX && 
							(ctr->usedRegisters & BA_CTRREG_RDX)) ||
						(reg == BA_IM_RDX && 
							(ctr->usedRegisters & BA_CTRREG_RAX)));

					if (isPushOpResReg) {
						ba_AddIM(ctr, 2, BA_IM_PUSH, opResultReg);
						ctr->imStackSize += 8;
					}

					if (lhs->lexemeType == BA_TK_GLOBALID) {
						ba_AddIM(ctr, 4, BA_IM_MOV, opResultReg, BA_IM_DATASGMT, 
							((struct ba_STVal*)lhs->val)->address);
					}
					else {
						ba_AddIM(ctr, 5, BA_IM_MOV, opResultReg, BA_IM_ADRADD, 
							ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, lhsOffset);
					}

					if (opLex == BA_TK_LSHIFTEQ || opLex == BA_TK_RSHIFTEQ) {
						if (reg != BA_IM_RCX) {
							if (ctr->usedRegisters & BA_CTRREG_RCX) {
								ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
								ctr->imStackSize += 8;
							}
							ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, reg);
						}

						ba_AddIM(ctr, 3, imOp, opResultReg, BA_IM_CL);

						if (reg != BA_IM_RCX && (ctr->usedRegisters & BA_CTRREG_RCX)) {
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
							ctr->imStackSize -= 8;
						}
					}
					else {
						ba_AddIM(ctr, 3, imOp, opResultReg, 
							reg ? reg : BA_IM_RCX);
					}

					ba_AddIM(ctr, 3, BA_IM_MOV, reg ? reg : BA_IM_RCX, 
						opResultReg);

					if (isPushOpResReg) {
						ba_AddIM(ctr, 2, BA_IM_POP, opResultReg);
						ctr->imStackSize -= 8;
					}
				}

				if (lhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_DATASGMT, 
						((struct ba_STVal*)lhs->val)->address, 
						reg ? reg : BA_IM_RCX);
				}
				else {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, 
						ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						lhsOffset, reg ? reg : BA_IM_RCX);
				}

				if (reg) {
					arg->lexemeType = BA_TK_IMREGISTER;
					arg->val = (void*)reg;
				}
				else {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
						stackPos, BA_IM_RCX);
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
					ctr->imStackSize -= 16;

					arg->lexemeType = BA_TK_IMRBPSUB;
					arg->val = (void*)ctr->imStackSize;
				}

				arg->type = lhs->type;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
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

				bool areBothUnsigned = ba_IsTypeUnsigned(lhs->type) && 
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

				arg->type = BA_TYPE_BOOL;
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
					if (!ctr->imStackSize) {
						ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, 
							BA_IM_RSP);
					}
					lhsStackPos = ctr->imStackSize + 8;
					// First: result location, second: preserve rax
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
					ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
					ctr->imStackSize += 16;
				}
				
				/* regR is replaced with rdx normally, but if regL is 
				 * already rdx, regR will be set to rcx. */
				u64 realRegL = regL ? regL : BA_IM_RAX;
				u64 realRegR = regR ? regR : 
					(regL == BA_IM_RDX ? BA_IM_RCX : BA_IM_RDX);

				if (!regR) {
					// Only once to preserve rdx/rcx
					ba_AddIM(ctr, 2, BA_IM_PUSH, realRegR);
					ctr->imStackSize += 8;
				}

				if (lhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(ctr, 4, BA_IM_MOV, realRegL,
						BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
				}
				else if (lhs->lexemeType == BA_TK_LOCALID) {
					ba_AddIM(ctr, 5, BA_IM_MOV, realRegL,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, lhs->val));
				}
				else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(ctr, 5, BA_IM_MOV, realRegL,
						BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
				}
				else if (isLhsLiteral) {
					ba_AddIM(ctr, 4, BA_IM_MOV, realRegL,
						BA_IM_IMM, (u64)lhs->val);
				}

				if (rhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(ctr, 4, BA_IM_MOV, realRegR,
						BA_IM_DATASGMT, ((struct ba_STVal*)rhs->val)->address);
				}
				else if (rhs->lexemeType == BA_TK_GLOBALID) {
					ba_AddIM(ctr, 4, BA_IM_MOV, realRegR,
						BA_IM_ADRADD, ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP,
						ba_CalcSTValOffset(ctr->currScope, rhs->val));
				}
				else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(ctr, 5, BA_IM_MOV, realRegR,
						BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
				}
				else if (isRhsLiteral) {
					ba_AddIM(ctr, 4, BA_IM_MOV, realRegR,
						BA_IM_IMM, (u64)rhs->val);
				}

				u64 movReg = (u64)ba_StkTop(ctr->cmpRegStk);
				!movReg && (movReg = realRegL);

				ba_AddIM(ctr, 3, BA_IM_CMP, realRegL, realRegR);
				ba_AddIM(ctr, 2, imOpSet, movReg - BA_IM_RAX + BA_IM_AL);
				ba_AddIM(ctr, 3, BA_IM_MOVZX, movReg, 
					movReg - BA_IM_RAX + BA_IM_AL);

				if (regL) {
					arg->lexemeType = BA_TK_IMREGISTER;
					arg->val = (void*)realRegL;
				}
				else {
					arg->lexemeType = BA_TK_IMRBPSUB;
					arg->val = (void*)ctr->imStackSize;

					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
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

					ba_AddIM(ctr, 2, imOpJcc, 
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
						ba_AddIM(ctr, 2, BA_IM_POP, realRegR);
						ctr->imStackSize -= 8;
					}

					if (ba_StkTop(ctr->cmpLblStk)) {
						ba_AddIM(ctr, 2, BA_IM_LABEL, 
							(u64)ba_StkPop(ctr->cmpLblStk));
					}
					ctr->cmpRegStk->items[ctr->cmpRegStk->count-1] = (void*)0;
				}

				if (!regL) {
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
					ctr->imStackSize -= 8;
				}

				return 1;
			}

			break;
		}

		case BA_OP_POSTFIX:
		{
			// This should never occur
			if (op->lexemeType == ')') {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, op->col);
			}
			// Func call
			else if (op->lexemeType == '(') {
				u64 funcArgsCnt = (u64)arg - 1;

				struct ba_Stk* argsStk = ba_NewStk();
				for (u64 i = 0; i < funcArgsCnt; i++) {
					ba_StkPush(argsStk, ba_StkPop(ctr->pTkStk));
				}
				
				struct ba_PTkStkItem* funcTk = ba_StkPop(ctr->pTkStk);
				if (!funcTk || funcTk->type != BA_TYPE_FUNC) {
					return ba_ExitMsg(BA_EXIT_ERR, "attempt to call "
						"non-func on", op->line, op->col);
				}

				struct ba_Func* func = 
					((struct ba_STVal*)funcTk->val)->initVal;
				func->isCalled = 1;

				if (!funcArgsCnt && func->paramCnt == 1 && 
					func->firstParam->hasDefaultVal)
				{
					struct ba_PTkStkItem* defaultParam = 
						malloc(sizeof(*defaultParam));
					defaultParam->val = func->firstParam->defaultVal;
					defaultParam->type = func->firstParam->type;
					defaultParam->lexemeType = BA_TK_LITINT;
					defaultParam->isLValue = 0;

					ba_StkPush(argsStk, defaultParam);
					funcArgsCnt = 1;
				}
				
				if (funcArgsCnt != func->paramCnt) {
					fprintf(stderr, "Error: func on line %llu:%llu "
						"takes %llu parameter%s, but %llu argument%s passed "
						"(including implicits)\n", op->line, op->col, 
						func->paramCnt, func->paramCnt == 1 ? "" : "s", 
						funcArgsCnt, funcArgsCnt == 1 ? " was" : "s were");
					exit(-1);
				}
				
				struct ba_FuncParam* param = func->firstParam;
				u64 originalImStackSize = ctr->imStackSize;

				while (argsStk->count) {
					struct ba_PTkStkItem* funcArg = ba_StkPop(argsStk);
					if (funcArg) {
						bool isArgNum = ba_IsTypeNumeric(funcArg->type);
						bool isParamNum = ba_IsTypeNumeric(param->type);
						if ((isArgNum ^ isParamNum) || 
							(!isArgNum && !isParamNum && 
							funcArg->type != param->type))
						{
							return fprintf(stderr, "Error: argument passed to "
								"func on line %llu:%llu has invalid type (%s) "
								"for parameter of type %s", op->line, op->col,
								ba_GetTypeStr(funcArg->type), 
								ba_GetTypeStr(param->type));
						}
						
						u64 reg = (u64)funcArg->val;
						if (funcArg->lexemeType != BA_TK_IMREGISTER) {
							reg = ba_NextIMRegister(ctr);
						}

						if (!reg) {
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ctr->imStackSize += 16;
						}
						
						if (ba_IsLexemeLiteral(funcArg->lexemeType)) {
							ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_IMM, 
								(u64)funcArg->val);
						}
						else if (funcArg->lexemeType == BA_TK_IMRBPSUB) {
							ba_AddIM(ctr, 5, BA_IM_MOV, reg, 
								BA_IM_ADRSUB, BA_IM_RBP, (u64)funcArg->val);
						}
						else if (funcArg->lexemeType == BA_TK_GLOBALID) {
							ba_AddIM(ctr, 4, BA_IM_MOV, reg, 
								BA_IM_DATASGMT, 
								((struct ba_STVal*)funcArg->val)->address);
						}
						else if (funcArg->lexemeType == BA_TK_LOCALID) {
							ba_AddIM(ctr, 5, BA_IM_MOV, reg, BA_IM_ADRADD, 
								ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
								ba_CalcSTValOffset(ctr->currScope, funcArg->val));
						}

						if (reg) {
							ba_AddIM(ctr, 2, BA_IM_PUSH, reg);
							ctr->imStackSize += 8;
						}
						else {
							ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD,
								BA_IM_RSP, 0x8, BA_IM_RAX);
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
							ctr->imStackSize -= 8;
						}
					}
					// Default param
					else {
						if (!param->hasDefaultVal) {
							return ba_ExitMsg2(BA_EXIT_ERR, "func called on", 
								op->line, op->col, "with implicit argument"
								"for parameter that has no default");
						}
						u64 reg = ba_NextIMRegister(ctr);
						if (reg) {
							ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_IMM, 
								(u64)param->defaultVal);
							ba_AddIM(ctr, 2, BA_IM_PUSH, reg);
							ctr->imStackSize += 8;
						}
						else {
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, 
								BA_IM_IMM, (u64)param->defaultVal);
							ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, 
								BA_IM_RSP, 0x8, BA_IM_RAX);
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
							ctr->imStackSize += 8;
						}
					}

					param = param->next;
				}

				ba_DelStk(argsStk);
				ctr->imStackSize = originalImStackSize;

				// TODO: Preserve+restore registers (ctr->usedRegisters)

				ba_AddIM(ctr, 2, BA_IM_LABELCALL, func->lblStart);

				// Clear func args from the stack
				if (func->paramStackSize) {
					ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, 
						BA_IM_IMM, func->paramStackSize);
				}

				// Get return value from rax
				// TODO: or from stack if too big
				struct ba_PTkStkItem* retVal = malloc(sizeof(*retVal));
				retVal->val = (void*)BA_IM_RAX;
				retVal->type = func->retType;
				retVal->lexemeType = BA_TK_IMREGISTER;
				retVal->isLValue = 0;
				ba_StkPush(ctr->pTkStk, retVal);

				return 2; // Because it is a left parenthesis
			}
			else if (op->lexemeType == '~') {
				struct ba_PTkStkItem* castedExp = ba_StkPop(ctr->pTkStk);
				if (!castedExp) {
					return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", 
						op->line, op->col);
				}
				struct ba_PTkStkItem* typeArg = arg;

				u64 newType = ba_GetTypeFromKeyword(typeArg->lexemeType);

				if (!newType || newType == BA_TYPE_VOID) {
					return ba_ExitMsg(BA_EXIT_ERR, "cast to expression that is "
						"not a (castable) type on", op->line, op->col);
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
				return 1;
			}

			break;
		}
	}
	
	return 1;
}

#endif
