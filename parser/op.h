// See LICENSE for copyright/license information

#ifndef BA__PARSER_OP_H
#define BA__PARSER_OP_H

#include "../lexer.h"
#include "../bltin/bltin.h"
#include "../common/reg.h"
#include "../common/parserstacks.h"

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
				op->lexemeType == '$' || op->lexemeType == '&' || 
				op->lexemeType == BA_TK_INC || op->lexemeType == BA_TK_DEC)
			{
				return 1;
			}
			else if (op->lexemeType == '(' || op->lexemeType == '[') {
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
			if (op->lexemeType == ')' || op->lexemeType == ']') {
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

/* Helper func for assigning intermediate registers or stack memory.
 * stackPos can be 0 for it to not be stored. */
void ba_POpAsgnRegOrStack(struct ba_Controller* ctr, u64 lexType, u64* reg, 
	u64* stackPos) 
{
	// *reg == 0 means on the stack, which is handled outside of this func
	(lexType != BA_TK_IMREGISTER) && (*reg = ba_NextIMRegister(ctr));

	if (!*reg) {
		if (!ctr->imStackSize) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
		}
		stackPos && (*stackPos = ctr->imStackSize + 8);
		// First: result location, second: preserve rax
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ctr->imStackSize += 16;
	}
}

void ba_POpMovIdToReg(struct ba_Controller* ctr, struct ba_STVal* id, 
	u64 argSize, u64 reg, bool isLea)
{
	ba_AddIM(ctr, 5, isLea ? BA_IM_LEA : BA_IM_MOV, 
		ba_AdjRegSize(reg, argSize), BA_IM_ADRADD, 
		ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
		ba_CalcSTValOffset(ctr->currScope, id));
}

u8 ba_POpMovArgToReg(struct ba_Controller* ctr, struct ba_PTkStkItem* arg, 
	u64 reg, bool isLiteral) 
{
	u64 argSize = ba_GetSizeOfType(arg->typeInfo);

	// Arrays are resolved as pointers to their starting location
	bool isArr = arg->typeInfo.type == BA_TYPE_ARR;
	if (isArr) {
		argSize = ba_GetSizeOfType((struct ba_Type){
			.type = BA_TYPE_PTR,
			.extraInfo = (void*)0
		});
	}

	if (arg->lexemeType == BA_TK_IDENTIFIER) {
		bool isSigned = ba_IsTypeSigned(arg->typeInfo.type);
		if (!isSigned && (argSize < 4)) {
			ba_AddIM(ctr, 3, BA_IM_XOR, reg, reg);
		}
		ba_POpMovIdToReg(ctr, arg->val, argSize, reg, /* isLea = */ isArr);
		if (isSigned && (argSize < 8)) {
			ba_AddIM(ctr, 3, BA_IM_MOVZX, reg, ba_AdjRegSize(reg, argSize));
		}
		return 1;
	}
	if (arg->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, isArr ? BA_IM_LEA : BA_IM_MOV, reg, 
			BA_IM_ADRSUB, BA_IM_RBP, (u64)arg->val);
		return 1;
	}
	if (isLiteral) {
		// TODO: handle array literals
		if (isArr) {
			return 0;
		}
		ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_IMM, 
			argSize < 8 ? (u64)arg->val & ((1llu << (argSize*8))-1)
				: (u64)arg->val);
		return 1;
	}
	return 0;
}

void ba_POpSetArg(struct ba_Controller* ctr, struct ba_PTkStkItem* arg, 
	u64 reg, u64 stackPos) 
{
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
	u64 argReg = (u64)arg->val; // Kept only if arg is a register
	ba_POpAsgnRegOrStack(ctr, arg->lexemeType, &argReg, &stackPos);
	u64 reg = argReg ? argReg : BA_IM_RAX;
	ba_POpMovArgToReg(ctr, arg, reg, /* isLiteral = */ 0);

	if (opLexType == '!') {
		u64 adjSizeReg = ba_AdjRegSize(reg, ba_GetSizeOfType(arg->typeInfo));
		ba_AddIM(ctr, 3, BA_IM_TEST, adjSizeReg, adjSizeReg);
		ba_AddIM(ctr, 2, BA_IM_SETZ, reg - BA_IM_RAX + BA_IM_AL);
		ba_AddIM(ctr, 3, BA_IM_MOVZX, reg, reg - BA_IM_RAX + BA_IM_AL);
	}
	else {
		ba_AddIM(ctr, 2, imOp, reg);
	}

	ba_POpSetArg(ctr, arg, argReg, stackPos);
}

// Handle binary operators with a non literal operand
void ba_POpNonLitBinary(struct ba_Controller* ctr, u64 imOp, 
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	struct ba_PTkStkItem* arg, bool isLhsLiteral, bool isRhsLiteral, 
	bool isShortCirc)
{
	u64 lhsStackPos = 0;
	u64 regL = (u64)lhs->val; // Kept only if lhs is a register
	ba_POpAsgnRegOrStack(ctr, lhs->lexemeType, &regL, &lhsStackPos);
	u64 realRegL = regL ? regL : BA_IM_RAX;
	ba_POpMovArgToReg(ctr, lhs, realRegL, isLhsLiteral);

	u64 regR = (u64)rhs->val; // Kept only if rhs is a register
	ba_POpAsgnRegOrStack(ctr, rhs->lexemeType, &regR, 0);
	/* regR is replaced with rdx normally, but if regL is 
	 * already rdx, regR will be set to rcx. */
	u64 rhsReplacement = regL == BA_IM_RDX ? BA_IM_RCX : BA_IM_RDX;
	if (!regR) {
		// Only once to preserve rdx/rcx
		ba_AddIM(ctr, 2, BA_IM_PUSH, rhsReplacement);
		ctr->imStackSize += 8;
	}
	u64 realRegR = regR ? regR : rhsReplacement;
	ba_POpMovArgToReg(ctr, rhs, realRegR, isRhsLiteral);

	ba_AddIM(ctr, 3, imOp, realRegL, realRegR);

	if (isShortCirc) {
		ba_AddIM(ctr, 2, BA_IM_LABEL, ba_StkPop(ctr->shortCircLblStk));
		ba_AddIM(ctr, 2, BA_IM_SETNZ, realRegL - BA_IM_RAX + BA_IM_AL);
		ba_AddIM(ctr, 3, BA_IM_MOVZX, realRegL,
			realRegL - BA_IM_RAX + BA_IM_AL);
	}

	if (regR) {
		ctr->usedRegisters &= ~ba_IMToCtrReg(regR);
	}
	else {
		ba_AddIM(ctr, 2, BA_IM_POP, rhsReplacement);
		ctr->imStackSize -= 8;
	}

	ba_POpSetArg(ctr, arg, regL, lhsStackPos);
}

// Handle bit shifts with at least one non literal operand
void ba_POpNonLitBitShift(struct ba_Controller* ctr, u64 imOp, 
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	struct ba_PTkStkItem* arg, bool isRhsLiteral) 
{
	u64 lhsStackPos = 0;
	u64 argRegL = (u64)lhs->val; // Kept only if lhs is a register

	// Return 0 means on the stack
	(lhs->lexemeType != BA_TK_IMREGISTER) && (argRegL = ba_NextIMRegister(ctr));

	if (argRegL == BA_IM_RCX) {
		argRegL = ba_NextIMRegister(ctr);
		ctr->usedRegisters &= ~BA_CTRREG_RCX;
		if (lhs->lexemeType == BA_TK_IMREGISTER) {
			ba_AddIM(ctr, 3, BA_IM_MOV, argRegL, BA_IM_RCX);
		}
	}

	if (!argRegL) {
		if (!ctr->imStackSize) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
		}
		lhsStackPos = ctr->imStackSize + 8;
		// First: result location, second: preserve rax
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ctr->imStackSize += 16;
	}

	u64 regL = argRegL ? argRegL : BA_IM_RAX;
	ba_POpMovArgToReg(ctr, lhs, regL, 
		/* isLiteral = */ lhs->lexemeType != BA_TK_IMREGISTER);

	if (isRhsLiteral) {
		ba_AddIM(ctr, 4, imOp, regL, BA_IM_IMM, (u64)rhs->val & 0x3f);
	}
	else {
		u64 regTmp = BA_IM_RCX;
		bool isRhsRcx = rhs->lexemeType == BA_TK_IMREGISTER &&
			(u64)rhs->val == BA_IM_RCX;

		if ((ctr->usedRegisters & BA_CTRREG_RCX) && !isRhsRcx) {
			regTmp = ba_NextIMRegister(ctr);
			if (regTmp) {
				ba_AddIM(ctr, 3, BA_IM_MOV, regTmp, BA_IM_RCX);
			}
			else {
				// Don't need to store rhs, only preserve rcx
				ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
				ctr->imStackSize += 8;
			}
		}

		ba_POpMovArgToReg(ctr, rhs, BA_IM_RCX, /* isLiteral = */ 0);

		if (rhs->lexemeType == BA_TK_IMREGISTER && !isRhsRcx) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, (u64)rhs->val);
			ctr->usedRegisters &= ~ba_IMToCtrReg((u64)rhs->val);
		}

		// The actual shift operation
		ba_AddIM(ctr, 3, imOp, regL, BA_IM_CL);

		if (!regTmp) {
			ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
			ctr->imStackSize -= 8;
		}
		else if (regTmp != BA_IM_RCX) {
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, regTmp);
			ctr->usedRegisters &= ~ba_IMToCtrReg(regTmp);
		}
	}

	ba_POpSetArg(ctr, arg, argRegL, lhsStackPos);
}

// Right associative operators don't handle operators of the same precedence
u8 ba_POpIsRightAssoc(struct ba_POpStkItem* op) {
	if (op->syntax == BA_OP_INFIX) {
		return op->lexemeType == '=' || ba_IsLexemeCompoundAssign(op->lexemeType);
	}
	else if (op->syntax == BA_OP_PREFIX || op->syntax == BA_OP_POSTFIX) {
		return op->lexemeType == '(' || op->lexemeType == '[';
	}
	return 0;
}

// Correct the type of dereferenced pointers/arrays
u8 ba_PCorrectDPtr(struct ba_Controller* ctr, struct ba_PTkStkItem* item) {
	if (item->typeInfo.type != BA_TYPE_DPTR) {
		return 1;
	}
	item->typeInfo = *(struct ba_Type*)item->typeInfo.extraInfo;
	u64 size = ba_GetSizeOfType(item->typeInfo);
	if (!size) {
		return 0;
	}
	if (item->typeInfo.type == BA_TYPE_ARR) {
		return 1;
	}
	if (item->lexemeType == BA_TK_IMREGISTER) {
		u64 adjReg = ba_AdjRegSize((u64)item->val, size);
		ba_AddIM(ctr, 4, BA_IM_MOV, adjReg, BA_IM_ADR, (u64)item->val);
		if (size < 8) {
			ba_AddIM(ctr, 3, BA_IM_MOVZX, (u64)item->val, adjReg);
		}
	}
	else if (item->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, BA_IM_ADRSUB, 
			BA_IM_RBP, (u64)item->val);
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
			(u64)item->val, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
	}
	return 1;
}

u8 ba_POpAssignChecks(struct ba_Controller* ctr, struct ba_Type lhsType, 
	struct ba_PTkStkItem* rhs, u64 line, u64 col) 
{
	if (lhsType.type == BA_TYPE_FUNC) {
		return ba_ExitMsg(BA_EXIT_ERR, "cannot assign func directly to another "
			"func,", line, col, ctr->currPath);
	}
	else if (lhsType.type == BA_TYPE_ARR && rhs->typeInfo.type == BA_TYPE_ARR && 
		ba_GetSizeOfType(lhsType) == ba_GetSizeOfType(rhs->typeInfo))
	{
		return 1;
	}
	else if (ba_IsTypeNumeric(lhsType.type) && 
		ba_IsTypeNumeric(rhs->typeInfo.type)) 
	{
		if (lhsType.type == BA_TYPE_PTR) {
			// 0 for null pointer is fine
			if (ba_IsLexemeLiteral(rhs->lexemeType) && (u64)rhs->val == 0) {
				return 1;
			}
			if (rhs->typeInfo.type != BA_TYPE_PTR) {
				ba_ExitMsg(BA_EXIT_WARN, "assignment of numeric non-pointer "
					"to pointer on", line, col, ctr->currPath);
			}
			else if (BA_TYPE_VOID != ((struct ba_Type*)lhsType.extraInfo)->type && 
				BA_TYPE_VOID != ((struct ba_Type*)rhs->typeInfo.extraInfo)->type && 
				!ba_AreTypesEqual(lhsType, rhs->typeInfo))
			{
				return ba_ExitMsg(BA_EXIT_WARN, "assignment of pointer to non-void " 
					"pointer of different type on", line, col, ctr->currPath);
			}
		}
		return 1;
	}
	else if (ba_AreTypesEqual(lhsType, rhs->typeInfo)) {
		return 1;
	}
	return ba_ErrorAssignTypes(line, col, ctr->currPath, lhsType, rhs->typeInfo);
}

void ba_POpFuncCallPushArgReg(struct ba_Controller* ctr, u64 reg, u64 size) {
	if (size == 8) {
		ba_AddIM(ctr, 2, BA_IM_PUSH, reg);
	}
	else if (size == 1) {
		ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RSP);
	}
	else {
		ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, size);
	}

	if (size < 8) {
		ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RSP, 
			ba_AdjRegSize(reg, size));
	}
}

void ba_PAssignArr(struct ba_Controller* ctr, struct ba_PTkStkItem* destItem, 
	struct ba_PTkStkItem* srcItem, u64 size)
{
	if (!ba_BltinFlagsTest(BA_BLTIN_MemCopy)) {
		ba_BltinMemCopy(ctr);
	}

	// Setting the default register so that it does not trample on used ones
	u64 defaultReg = BA_IM_RAX;
	u64 d = 0;
	destItem && destItem->lexemeType == BA_TK_IMREGISTER && 
		(d = (u64)destItem->val);
	u64 s = srcItem->lexemeType == BA_TK_IMREGISTER ? (u64)srcItem->val : 0;
	if (d == BA_IM_RAX || s == BA_IM_RAX) {
		defaultReg = (d == BA_IM_RCX || s == BA_IM_RCX) ? BA_IM_RDX : BA_IM_RCX;
	}
	u64 reg = defaultReg;

	// Destination pointer
	if (!destItem) {
		reg = BA_IM_RSP;
	}
	else if (destItem->lexemeType == BA_TK_IDENTIFIER) {
		ba_AddIM(ctr, 5, BA_IM_LEA, defaultReg, BA_IM_ADRADD, BA_IM_RSP, 
			ba_CalcSTValOffset(ctr->currScope, destItem->val));
	}
	// (DPTR)
	else if (destItem->lexemeType == BA_TK_IMREGISTER) {
		ba_AddIM(ctr, 4, BA_IM_MOV, defaultReg, BA_IM_ADR, (u64)destItem->val);
	}
	else if (destItem->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, BA_IM_MOV, defaultReg, BA_IM_ADRSUB, 
			BA_IM_RBP, (u64)destItem->val);
	}
	ba_AddIM(ctr, 3, BA_IM_PUSH, reg);

	// Source pointer
	reg = defaultReg;
	if (srcItem->lexemeType == BA_TK_IDENTIFIER) {
		ba_AddIM(ctr, 5, BA_IM_LEA, defaultReg, BA_IM_ADRADD, BA_IM_RSP, 
			ba_CalcSTValOffset(ctr->currScope, srcItem->val) + 8);
	}
	else if (srcItem->lexemeType == BA_TK_IMREGISTER) {
		reg = (u64)srcItem->val;
	}
	else if (ba_IsLexemeLiteral(srcItem->lexemeType)) {
		// TODO
	}
	ba_AddIM(ctr, 2, BA_IM_PUSH, reg); // src ptr
	
	ba_AddIM(ctr, 4, BA_IM_MOV, defaultReg, BA_IM_IMM, size); // mem size
	ba_AddIM(ctr, 2, BA_IM_PUSH, defaultReg);
	ba_AddIM(ctr, 2, BA_IM_LABELCALL, ba_BltinLabels[BA_BLTIN_MemCopy]);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 0x18);
}

// Handle operations (i.e. perform operation now or generate code for it)
u8 ba_POpHandle(struct ba_Controller* ctr, struct ba_POpStkItem* handler) {
	struct ba_POpStkItem* op = ba_StkPop(ctr->pOpStk);
	struct ba_PTkStkItem* arg = ba_StkPop(ctr->pTkStk);
	if (!arg) {
		return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, op->col, 
			ctr->currPath);
	}
	
	switch (op->syntax) {
		case BA_OP_PREFIX:
		{
			// Try to correct dereferenced pointers, and throw error if invalid
			op->lexemeType != '&' && op->lexemeType != BA_TK_INC && 
				op->lexemeType != BA_TK_DEC && op->lexemeType != '[' && 
				!ba_PCorrectDPtr(ctr, arg) && 
				ba_ErrorDerefInvalid(op->line, op->col, ctr->currPath);

			if (op->lexemeType == '+') {
				if (!ba_IsTypeNumeric(arg->typeInfo.type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "unary '+' used with non "
						"numeric operand on", op->line, op->col, ctr->currPath);
				}

				arg->typeInfo.type = ba_IsTypeSigned(arg->typeInfo.type) 
					? BA_TYPE_I64 : BA_TYPE_U64;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			else if (op->lexemeType == '$') {
				arg->val = (void*)ba_GetSizeOfType(arg->typeInfo);

				if (!arg->val) {
					return ba_ExitMsg(BA_EXIT_ERR, "type of operand to '$' "
						"operator has undefined size on", op->line, op->col, 
						ctr->currPath);
				}

				arg->typeInfo.type = BA_TYPE_U64;
				arg->lexemeType = BA_TK_LITINT;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			else if (op->lexemeType == '&') {
				if (!arg->isLValue) {
					return ba_ExitMsg(BA_EXIT_ERR, "cannot get address of "
						"non-lvalue on", op->line, op->col, ctr->currPath);
				}

				// TODO: add feature
				if (arg->typeInfo.type == BA_TYPE_FUNC) {
					return ba_ExitMsg(BA_EXIT_ERR, "cannot make pointer to "
						"func on", op->line, op->col, ctr->currPath);
				}
				
				u64 stackPos = 0;
				u64 argReg = (u64)arg->val; // Only kept if arg is a register
				ba_POpAsgnRegOrStack(ctr, arg->lexemeType, &argReg, &stackPos);
				u64 reg = argReg ? argReg : BA_IM_RAX;

				if (arg->lexemeType == BA_TK_IDENTIFIER) {
					ba_POpMovIdToReg(ctr, arg->val, /* argSize = */ 8, 
						reg, /* isLea = */ 1);
				}
				// IMRBPSUB must be a DPTR
				else if (arg->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(ctr, 5, BA_IM_MOV, reg, BA_IM_ADRSUB, 
						BA_IM_RBP, (u64)arg->val);
				}
				
				if (arg->lexemeType == BA_TK_IDENTIFIER) {
					struct ba_Type* origType = malloc(sizeof(*origType));
					if (!origType) {
						return ba_ErrorMallocNoMem();
					}
					memcpy(origType, &arg->typeInfo, sizeof(*origType));
					arg->typeInfo.extraInfo = origType;
				}
				
				ba_POpSetArg(ctr, arg, argReg, stackPos);

				arg->typeInfo.type = BA_TYPE_PTR;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			else if (op->lexemeType == '-' || op->lexemeType == '~' ||
				op->lexemeType == '!')
			{
				if ((op->lexemeType == '~' && 
					!ba_IsTypeIntegral(arg->typeInfo.type)) ||
					!ba_IsTypeNumeric(arg->typeInfo.type))
				{
					fprintf(stderr, "Error: unary '%s' used with non %s "
						"operand on line %llu:%llu in %s", 
						ba_GetLexemeStr(op->lexemeType), 
						op->lexemeType == '~' ? "integral" : "numeric", 
						op->line, op->col, ctr->currPath);
					exit(-1);
				}

				u64 imOp;
				((op->lexemeType == '-') && (imOp = BA_IM_NEG)) ||
				((op->lexemeType == '~') && (imOp = BA_IM_NOT));

				if (ba_IsTypeIntegral(arg->typeInfo.type)) {
					if (ba_IsLexemeLiteral(arg->lexemeType)) {
						u64 size = ba_GetSizeOfType(arg->typeInfo);
						(size < 8) && (arg->val = 
							(void*)((u64)arg->val & ((1llu<<(size*8))-1)));
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
					arg->typeInfo.type = BA_TYPE_BOOL;
				}
				else if (ba_IsTypeUnsigned(arg->typeInfo.type)) {
					arg->typeInfo.type = BA_TYPE_U64;
				}
				else if (ba_IsTypeSigned(arg->typeInfo.type)) {
					arg->typeInfo.type = BA_TYPE_I64;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			else if (op->lexemeType == BA_TK_INC || 
				op->lexemeType == BA_TK_DEC)
			{
				if (!arg->isLValue) {
					return ba_ExitMsg(BA_EXIT_ERR, "increment/decrement of "
						"non-lvalue on", op->line, op->col, ctr->currPath);
				}

				// Correct dereferenced pointers
				if (arg->typeInfo.type == BA_TYPE_DPTR) {
					arg->typeInfo = *(struct ba_Type*)arg->typeInfo.extraInfo;
				}

				u64 argSize = ba_GetSizeOfType(arg->typeInfo);

				if (!ba_IsTypeNumeric(arg->typeInfo.type)) {
					return ba_ExitMsg(BA_EXIT_ERR, "increment of non-numeric "
						"lvalue on", op->line, op->col, ctr->currPath);
				}

				u64 imOp = op->lexemeType == BA_TK_INC ? BA_IM_INC : BA_IM_DEC;
				u64 ptrImOp = 
					op->lexemeType == BA_TK_INC ? BA_IM_ADD : BA_IM_SUB;

				u64 argReg = 0;
				u64 stackPos = 0;
				/* In ba_POpAsgnRegOrStack, the parameter lexType is only used 
				 * to forgo allocating a new register if the lexeme is already
				 * one. Here we must allocate a new register in all cases. */
				u64 lexType = arg->lexemeType == BA_TK_IMREGISTER 
					? 0 : arg->lexemeType;
				ba_POpAsgnRegOrStack(ctr, lexType, &argReg, &stackPos);
				u64 reg = argReg ? argReg : BA_IM_RAX;

				if (arg->lexemeType == BA_TK_IDENTIFIER) {
					ba_POpMovIdToReg(ctr, arg->val, argSize, reg, 
						/* isLea = */ 0);
				}
				// IMREGISTER or IMRBPSUB must be a DPTR
				else if (arg->lexemeType == BA_TK_IMREGISTER) {
					ba_AddIM(ctr, 4, BA_IM_MOV, ba_AdjRegSize(reg, argSize), 
						BA_IM_ADR, (u64)arg->val);
				}
				else if (arg->lexemeType == BA_TK_IMRBPSUB) {
					u64 adrLocReg = reg == BA_IM_RAX ? BA_IM_RCX : BA_IM_RAX;
					ba_AddIM(ctr, 2, BA_IM_PUSH, adrLocReg);
					ba_AddIM(ctr, 5, BA_IM_MOV, adrLocReg, 
						BA_IM_ADRSUB, BA_IM_RBP, (u64)arg->val);
					ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_ADR, adrLocReg);
					ba_AddIM(ctr, 2, BA_IM_POP, adrLocReg);
				}

				if (arg->typeInfo.type == BA_TYPE_PTR && argSize != 1) {
					ba_AddIM(ctr, 4, ptrImOp, reg, BA_IM_IMM, argSize);
				}
				else {
					ba_AddIM(ctr, 2, imOp, reg);
				}

				if (arg->lexemeType == BA_TK_IDENTIFIER) {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, 
						ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, arg->val), 
						ba_AdjRegSize(reg, argSize));
				}
				// IMREGISTER or IMRBPSUB must be a DPTR
				else if (arg->lexemeType == BA_TK_IMREGISTER) {
					ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, (u64)arg->val,
						ba_AdjRegSize(reg, argSize));
				}
				else if (arg->lexemeType == BA_TK_IMRBPSUB) {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, 
						BA_IM_RBP, (u64)arg->val, reg);
				}

				ba_POpSetArg(ctr, arg, argReg, stackPos);
				if (!argReg) {
					// Account for the additional intermediate
					ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
					ctr->imStackSize -= 8;
					arg->val -= 8;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			else if (op->lexemeType == '(') {
				ba_StkPush(ctr->pTkStk, arg);
				/* IMPORTANT!!!!
				 * Return 2 leads to the parser moving back to parsing lexemes 
				 * as if it had just followed an atom. */
				return 2;
			}
			else if (op->lexemeType == '[') {
				ba_StkPush(ctr->pTkStk, arg);
				return 2; // Go back to parsing as if having followed an atom
			}
			break;
		}

		case BA_OP_INFIX:
		{
			struct ba_PTkStkItem* lhs = ba_StkPop(ctr->pTkStk);
			if (!lhs) {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, 
					op->col, ctr->currPath);
			}
			struct ba_PTkStkItem* rhs = arg;
			
			bool isLhsLiteral = ba_IsLexemeLiteral(lhs->lexemeType);
			bool isRhsLiteral = ba_IsLexemeLiteral(rhs->lexemeType);

			// Correct DPTR unless the operation is some kind of assignment
			op->lexemeType != '=' &&
				!ba_IsLexemeCompoundAssign(op->lexemeType) && 
				!ba_PCorrectDPtr(ctr, lhs) &&
				ba_ErrorDerefInvalid(op->line, op->col, ctr->currPath);

			// Bit shifts
			if (op->lexemeType == BA_TK_LSHIFT || 
				op->lexemeType == BA_TK_RSHIFT) 
			{
				if (!ba_IsTypeIntegral(lhs->typeInfo.type) || 
					!ba_IsTypeIntegral(rhs->typeInfo.type)) 
				{
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift used with non "
						"integral operand(s) on", op->line, op->col, 
						ctr->currPath);
				}

				arg->typeInfo.type = ba_IsTypeUnsigned(lhs->typeInfo.type) 
					? BA_TYPE_U64 : BA_TYPE_I64;
				
				// If rhs is 0, there is no change in in lhs
				if (isRhsLiteral && !rhs->val) {
					ba_StkPush(ctr->pTkStk, lhs);
					return 1;
				}

				if (isLhsLiteral && isRhsLiteral) {
					arg->val = (op->lexemeType == BA_TK_LSHIFT)
						? (void*)(((u64)lhs->val) << ((u64)rhs->val & 0x3f))
						: (void*)(((u64)lhs->val) >> ((u64)rhs->val & 0x3f));
				}
				else {
					u64 imOp = op->lexemeType == BA_TK_LSHIFT 
						? BA_IM_SHL : BA_IM_SHR;
					ba_POpNonLitBitShift(ctr, imOp, lhs, rhs, arg, isRhsLiteral);
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

				if (!ba_IsTypeNumeric(lhs->typeInfo.type) || 
					!ba_IsTypeNumeric(rhs->typeInfo.type)) 
				{
					char msg[128] = {0};
					strcat(msg, opName);
					strcat(msg, " with non numeric operand(s) on");
					return ba_ExitMsg(BA_EXIT_ERR, msg, op->line, op->col, 
						ctr->currPath);
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
				else if ((op->lexemeType == '*') && 
					(isLhsLiteral || isRhsLiteral)) 
				{
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
						ba_POpNonLitBinary(ctr, BA_IM_ADD, nonLitArg, nonLitArg,
							arg, /* isLhsLiteral = */ 0, /* isRhsLiteral = */ 0,
							/* isShortCirc = */ 0);
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
						newRhs->typeInfo.type = BA_TYPE_U64;
						newRhs->lexemeType = BA_TK_LITINT;

						ba_POpNonLitBitShift(ctr, BA_IM_SHL, nonLitArg, newRhs,
							arg, /* isRhsLiteral = */ 1);
					}
					else {
						goto BA_LBL_POPHANDLE_MULADDSUB_NONLIT;
					}
				}
				else {
					BA_LBL_POPHANDLE_MULADDSUB_NONLIT:
					ba_POpNonLitBinary(ctr, imOp, lhs, rhs, arg, isLhsLiteral, 
						isRhsLiteral, /* isShortCirc = */ 0);
				}

				if (ba_IsTypeIntegral(lhs->typeInfo.type) && 
					ba_IsTypeIntegral(rhs->typeInfo.type)) 
				{
					u64 argType = BA_TYPE_I64; // Default, most likely type
					if (ba_IsTypeUnsigned(lhs->typeInfo.type) ^ 
						ba_IsTypeUnsigned(rhs->typeInfo.type)) 
					{
						ba_WarnImplicitSignedConversion(op->line, op->col, 
							ctr->currPath, opName);
					}
					else if (ba_IsTypeUnsigned(lhs->typeInfo.type)) {
						argType = BA_TYPE_U64;
					}
					arg->typeInfo.type = argType;
				}

				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}

			// Integer division and modulo
			else if (op->lexemeType == BA_TK_IDIV || op->lexemeType == '%') {
				if (!ba_IsTypeNumeric(lhs->typeInfo.type) || 
					!ba_IsTypeNumeric(rhs->typeInfo.type)) 
				{
					return ba_ExitMsg(BA_EXIT_ERR, "integer division or modulo "
						"with non numeric operand(s) on", op->line, op->col, 
						ctr->currPath);
				}

				u64 argType = BA_TYPE_I64; // Default, most likely type
				bool areBothUnsigned = ba_IsTypeUnsigned(lhs->typeInfo.type) && 
					ba_IsTypeUnsigned(rhs->typeInfo.type);

				if (areBothUnsigned) {
					argType = BA_TYPE_U64;
				}
				// Different signedness
				else if (ba_IsTypeUnsigned(lhs->typeInfo.type) ^ 
					ba_IsTypeUnsigned(rhs->typeInfo.type)) 
				{
					ba_WarnImplicitSignedConversion(op->line, op->col, 
						ctr->currPath, 
						op->lexemeType == '%' ? "modulo" : "integer division");
				}
				
				u64 rhsSize = ba_GetSizeOfType(rhs->typeInfo);
				isRhsLiteral && rhsSize < 8 && 
					(rhs->val = 
					 	(void*)((u64)rhs->val & ((1llu<<(rhsSize*8))-1)));

				if (isLhsLiteral && isRhsLiteral) {
					u64 lhsSize = ba_GetSizeOfType(lhs->typeInfo);
					lhsSize < 8 && 
						(lhs->val = 
						 	(void*)((u64)lhs->val & ((1llu<<(lhsSize*8))-1)));
					
					if (op->lexemeType == '%') {
						u64 modVal;
						if (areBothUnsigned) {
							modVal = (u64)lhs->val % (u64)rhs->val;
						}
						else {
							modVal = ((i64)lhs->val % (i64)rhs->val) + 
								(((i64)lhs->val < 0) ^ ((i64)rhs->val < 0)) * 
								(i64)rhs->val;
						}
						arg->val = (void*)modVal;
					}
					else {
						arg->val = areBothUnsigned 
							? (void*)(((u64)lhs->val) / ((u64)rhs->val))
							: (void*)(((i64)lhs->val) / ((i64)rhs->val));
					}
				}
				else if (isRhsLiteral) {
					if (!rhs->val) {
						return ba_ExitMsg(BA_EXIT_ERR, "division or modulo by "
							"zero on", op->line, op->col, ctr->currPath);
					}

					// Test for sign
					bool isRhsNeg = ba_IsTypeSigned(rhs->typeInfo.type) && 
						((u64)rhs->val & (1llu << (rhsSize*8-1)));

					if (op->lexemeType == '%') { // Modulo
						u64 rhsAbs = isRhsNeg ? -(u64)rhs->val : (u64)rhs->val;
						if (rhsAbs == 1) {
							arg->val = 0;
							arg->lexemeType = BA_TK_LITINT;
						}
						else if (!(rhsAbs & (rhsAbs - 1))) {
							// rhs is a power of 2
							/* a % (1<<b) == a & ((1<<b)-1) 
							 * a % -(1<<b) == (a & ((1<<b)-1)) - (1<<b) */

							struct ba_PTkStkItem* newRhs = malloc(sizeof(*newRhs));
							if (!newRhs) {
								return ba_ErrorMallocNoMem();
							}
							newRhs->val = (void*)(rhsAbs-1);
							newRhs->typeInfo.type = BA_TYPE_U64;
							newRhs->lexemeType = BA_TK_LITINT;

							ba_POpNonLitBitShift(ctr, BA_IM_AND, lhs, newRhs, 
								arg, /* isRhsLiteral = */ 1);

							if (isRhsNeg) {
								newRhs->val = (void*)rhsAbs;
								newRhs->typeInfo.type = BA_TYPE_I64;
								newRhs->lexemeType = BA_TK_LITINT;
								ba_POpNonLitBinary(ctr, BA_IM_SUB, arg, newRhs, 
									arg, /* isLhsLiteral = */ 0, 
									/* isRhsLiteral = */ 1, 
									/* isShortCirc = */ 0);
							}
						}
						else {
							goto BA_LBL_POPHANDLE_INTDIVMOD_NONLIT;
						}
					}
					else { // Integer division
						if (isRhsNeg) {
							ba_POpNonLitUnary('-', lhs, ctr);
							rhs->val = rhsSize == 8 
								? (void*)(-(i64)rhs->val) 
								: (void*)((-(i64)rhs->val) & 
									((1llu<<(rhsSize*8))-1));
						}

						if ((u64)rhs->val == 1) {
							arg->val = lhs->val;
							arg->lexemeType = lhs->lexemeType;
						}
						else {
							goto BA_LBL_POPHANDLE_INTDIVMOD_NONLIT;
						}
					}
				}
				else {
					BA_LBL_POPHANDLE_INTDIVMOD_NONLIT:
					
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
						/* Only result location pushed, since rax will be 
						 * praeserved later */
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
						ctr->imStackSize += 8;
					}

					/* Make sure rax and rdx are not trampled on by the div
					 * instruction */
					u64 originalUsedRaxRdx = ctr->usedRegisters & 
						(BA_CTRREG_RAX | BA_CTRREG_RDX);
					ctr->usedRegisters |= BA_CTRREG_RAX | BA_CTRREG_RDX;

					if (rhs->lexemeType != BA_TK_IMREGISTER || 
						(u64)rhs->val == BA_IM_RAX || (u64)rhs->val == BA_IM_RDX)
					{
						regR = ba_NextIMRegister(ctr);
					}

					ctr->usedRegisters &= originalUsedRaxRdx |
						(ctr->usedRegisters & ~BA_CTRREG_RAX & ~BA_CTRREG_RDX);

					// [0] = rax, [1] = rdx
					bool isPushedFlags[2] = { 0, 0 };
					u64 tmpCtrRegs[2] = { BA_CTRREG_RAX, BA_CTRREG_RDX };
					u64 tmpRegs[2] = { BA_IM_RAX, BA_IM_RDX };

					for (u64 i = 0; i < 2; ++i) {
						if ((ctr->usedRegisters & tmpCtrRegs[i]) && 
							regL != tmpRegs[i]) 
						{
							if (rhs->lexemeType == BA_TK_IMREGISTER && 
								(u64)rhs->val == tmpRegs[i]) 
							{
								ba_AddIM(ctr, 3, BA_IM_MOV, regR, tmpRegs[i]);
								continue;
							}
							ba_AddIM(ctr, 2, BA_IM_PUSH, tmpRegs[i]);
							ctr->imStackSize += 8;
							isPushedFlags[i] = 1;
						}
					}

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrReg(regR);
					}
					else {
						ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
						ctr->imStackSize += 8;
					}

					ba_POpMovArgToReg(ctr, lhs, BA_IM_RAX, isLhsLiteral);

					if (op->lexemeType == BA_TK_IDIV && 
						lhs->lexemeType == BA_TK_IMREGISTER && 
						(u64)lhs->val != BA_IM_RAX) 
					{
						ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, (u64)lhs->val);
					}

					ba_POpMovArgToReg(ctr, rhs, regR ? regR : BA_IM_RCX, 
						isRhsLiteral);

					ba_AddIM(ctr, 1, BA_IM_CQO);

					u64 imOp = areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV;
					ba_AddIM(ctr, 2, imOp, regR ? regR : BA_IM_RCX);

					if (op->lexemeType == '%') {
						if (!areBothUnsigned) {
							/* Test result for sign: if negative, then operands 
							 * had opposite signs and so the rhs needs to be 
							 * added */
							
							// Sign stored in rax
							u64 raxAdj = ba_AdjRegSize(BA_IM_RAX, rhsSize);
							ba_AddIM(ctr, 3, BA_IM_TEST, raxAdj, raxAdj);
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
					}
					else {
						if (regL && regL != BA_IM_RAX) {
							ba_AddIM(ctr, 3, BA_IM_MOV, regL, BA_IM_RAX);
						}
						ba_POpSetArg(ctr, arg, regL, lhsStackPos);
					}

					// Restore registers

					if (isPushedFlags[1]) {
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

					if (isPushedFlags[0]) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						ctr->imStackSize -= 8;
					}
				}
				
				arg->typeInfo.type = argType;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			
			// Bitwise operations
			
			else if (op->lexemeType == '&' || op->lexemeType == '^' ||
				op->lexemeType == '|')
			{
				if (!ba_IsTypeIntegral(lhs->typeInfo.type) && 
					!ba_IsTypeIntegral(rhs->typeInfo.type)) 
				{
					return ba_ExitMsg(BA_EXIT_ERR, "bitwise operation used "
						"with non integral operand(s) on", op->line, op->col, 
						ctr->currPath);
				}

				u64 argType = (ba_IsTypeUnsigned(lhs->typeInfo.type) && 
					ba_IsTypeUnsigned(rhs->typeInfo.type))
						? BA_TYPE_U64
						: BA_TYPE_I64;
				arg->typeInfo.type = argType;

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
					ba_POpNonLitBinary(ctr, imOp, lhs, rhs, arg, isLhsLiteral, 
						isRhsLiteral, /* isShortCirc = */ 0);
				}
				
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			
			// Logical short-circuiting operators
			else if (op->lexemeType == BA_TK_LOGAND || 
				op->lexemeType == BA_TK_LOGOR) 
			{
				if (!ba_IsTypeNumeric(lhs->typeInfo.type) ||
					!ba_IsTypeNumeric(rhs->typeInfo.type))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "logical short-circuiting "
						"operation with non numeric operand(s) on", 
						op->line, op->col, ctr->currPath);
				}

				arg->typeInfo.type = BA_TYPE_BOOL;
				u64 imOp = op->lexemeType == BA_TK_LOGAND ? BA_IM_AND : BA_IM_OR;

				/* Although it is called "NonLit", it does work with literals 
				 * as well. */
				ba_POpNonLitBinary(ctr, imOp, lhs, rhs, arg, isLhsLiteral, 
					isRhsLiteral, /* isShortCirc = */ 1);
				
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}
			
			// Assignment
			// TODO: rewrite to clean up division and modulo
			else if (op->lexemeType == '=' || 
				ba_IsLexemeCompoundAssign(op->lexemeType)) 
			{
				if (!lhs->isLValue) {
					return ba_ExitMsg(BA_EXIT_ERR, "assignment to non-lvalue on", 
						op->line, op->col, ctr->currPath);
				}

				u64 opLex = op->lexemeType;
				struct ba_Type lhsType = lhs->typeInfo.type == BA_TYPE_DPTR
					? *(struct ba_Type*)lhs->typeInfo.extraInfo 
					: lhs->typeInfo;

				ba_POpAssignChecks(ctr, lhsType, rhs, op->line, op->col);

				if (opLex != '=' && (!ba_IsTypeNumeric(lhsType.type) ||
					!ba_IsTypeNumeric(rhs->typeInfo.type)))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "numeric operation "
						"used with non numeric operand(s) on", op->line,
						op->col, ctr->currPath);
				}

				if (lhsType.type == BA_TYPE_ARR) {
					ba_PAssignArr(ctr, lhs, rhs, ba_GetSizeOfType(lhs->typeInfo));
					arg->lexemeType = lhs->lexemeType;
					arg->val = lhs->val;
					arg->typeInfo = lhs->typeInfo;
					arg->isLValue = 0;
					ba_StkPush(ctr->pTkStk, arg);
					return 1;
				}

				if ((opLex == BA_TK_BITANDEQ || opLex == BA_TK_BITXOREQ || 
					opLex == BA_TK_BITOREQ || opLex == BA_TK_LSHIFTEQ || 
					opLex == BA_TK_RSHIFTEQ) && 
					(!ba_IsTypeIntegral(lhsType.type) || 
					!ba_IsTypeIntegral(rhs->typeInfo.type)))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "bit shift or bitwise "
						"operation used with non integral operand(s) on", 
						op->line, op->col, ctr->currPath);
				}

				bool isUsingDiv = opLex == BA_TK_IDIVEQ || opLex == BA_TK_MODEQ;
				// Different signedness
				if (isUsingDiv && (ba_IsTypeUnsigned(lhs->typeInfo.type) ^ 
					ba_IsTypeUnsigned(rhs->typeInfo.type)))
				{
					ba_WarnImplicitSignedConversion(
						op->line, op->col, ctr->currPath, 
						op->lexemeType == BA_TK_MODEQ 
							? "modulo" : "integer division");
				}

				// Praeserve registers used by DIV or IDIV instruction
				u64 originalUsedRaxRdx = ctr->usedRegisters & 
					(BA_CTRREG_RAX | BA_CTRREG_RDX);
				(isUsingDiv) &&
					(ctr->usedRegisters |= BA_CTRREG_RAX | BA_CTRREG_RDX);

				u64 stackPos = 0;
				u64 reg = (u64)rhs->val; // Kept only if rhs is a register

				if (rhs->lexemeType != BA_TK_IMREGISTER || (isUsingDiv && 
					((u64)rhs->val == BA_IM_RAX || (u64)rhs->val == BA_IM_RDX)))
				{
					reg = ba_NextIMRegister(ctr);
				}

				(isUsingDiv) &&
					(ctr->usedRegisters &= originalUsedRaxRdx |
						(ctr->usedRegisters & ~BA_CTRREG_RAX & ~BA_CTRREG_RDX));
				
				u64 realReg = reg;
				if (!reg) {
					if (!ctr->imStackSize) {
						ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
					}
					stackPos = ctr->imStackSize + 8;
					// First: result location, second: praeserve realReg
					realReg = (lhs->lexemeType == BA_TK_IMREGISTER && 
						(u64)lhs->val == BA_IM_RCX) 
							? BA_IM_RSI : BA_IM_RCX;
					ba_AddIM(ctr, 2, BA_IM_PUSH, realReg);
					ba_AddIM(ctr, 2, BA_IM_PUSH, realReg);
					ctr->imStackSize += 16;
				}

				ba_POpMovArgToReg(ctr, rhs, realReg, isRhsLiteral);

				u64 imOp = 0;
				((opLex == BA_TK_ADDEQ) && (imOp = BA_IM_ADD)) ||
				((opLex == BA_TK_SUBEQ) && (imOp = BA_IM_SUB)) ||
				((opLex == BA_TK_MULEQ) && (imOp = BA_IM_IMUL)) ||
				((opLex == BA_TK_LSHIFTEQ) && (imOp = BA_IM_SHL)) ||
				((opLex == BA_TK_RSHIFTEQ) && (imOp = BA_IM_SHR)) ||
				((opLex == BA_TK_BITANDEQ) && (imOp = BA_IM_AND)) ||
				((opLex == BA_TK_BITXOREQ) && (imOp = BA_IM_XOR)) ||
				((opLex == BA_TK_BITOREQ) && (imOp = BA_IM_OR));

				u64 lhsSize = ba_GetSizeOfType(lhsType);

				if (isUsingDiv) {
					if (isRhsLiteral && !rhs->val) {
						return ba_ExitMsg(BA_EXIT_ERR, "division or modulo by " 
							"zero on", op->line, op->col, ctr->currPath);
					}

					bool areBothUnsigned = ba_IsTypeUnsigned(lhsType.type) &&
						ba_IsTypeUnsigned(rhs->typeInfo.type);

					// [0] = rax, [1] = rdx
					bool isPushedFlags[2] = { 0, 0 };
					u64 tmpCtrRegs[2] = { BA_CTRREG_RAX, BA_CTRREG_RDX };
					u64 tmpRegs[2] = { BA_IM_RAX, BA_IM_RDX };

					for (u64 i = 0; i < 2; ++i) {
						if ((ctr->usedRegisters & tmpCtrRegs[i]) && 
							reg != tmpRegs[i]) 
						{
							if (rhs->lexemeType == BA_TK_IMREGISTER && 
								(u64)rhs->val == tmpRegs[i]) 
							{
								ba_AddIM(ctr, 3, BA_IM_MOV, reg, tmpRegs[i]);
								continue;
							}
							ba_AddIM(ctr, 2, BA_IM_PUSH, tmpRegs[i]);
							ctr->imStackSize += 8;
							isPushedFlags[i] = 1;
						}
					}

					u64 lhsRax = ba_AdjRegSize(BA_IM_RAX, lhsSize);
					if (lhs->lexemeType == BA_TK_IDENTIFIER) {
						ba_AddIM(ctr, 5, BA_IM_MOV, lhsRax, BA_IM_ADRADD, 
							ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
							ba_CalcSTValOffset(ctr->currScope, lhs->val));
					}
					// (DPTR)
					else if (lhs->lexemeType == BA_TK_IMREGISTER) {
						ba_AddIM(ctr, 4, BA_IM_MOV, lhsRax, 
							BA_IM_ADR, (u64)lhs->val);
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						u64 adrLocReg = 
							realReg == BA_IM_RDX ? BA_IM_RCX : BA_IM_RDX;
						ba_AddIM(ctr, 2, BA_IM_PUSH, adrLocReg);
						ba_AddIM(ctr, 5, BA_IM_MOV, adrLocReg, BA_IM_ADRSUB, 
							BA_IM_RBP, (u64)lhs->val);
						ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, 
							BA_IM_ADR, adrLocReg);
						ba_AddIM(ctr, 2, BA_IM_POP, adrLocReg);
					}

					ba_AddIM(ctr, 1, BA_IM_CQO);
					ba_AddIM(ctr, 2, areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV, 
						realReg);
					
					u64 divResultReg = BA_TK_IDIVEQ ? BA_IM_RAX : BA_IM_RDX;
					if (reg != divResultReg) {
						ba_AddIM(ctr, 3, BA_IM_MOV, realReg, divResultReg);
					}

					// Restore registers
					if (isPushedFlags[1]) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
						ctr->imStackSize -= 8;
					}
					if (isPushedFlags[0]) {
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						ctr->imStackSize -= 8;
					}
				}
				else if (imOp) {
					u64 opResultReg = 
						reg == BA_IM_RAX ? BA_IM_RDX : BA_IM_RAX;

					bool isPushOpResReg = 
						((opResultReg == BA_IM_RAX && 
							(ctr->usedRegisters & BA_CTRREG_RAX)) ||
						(opResultReg == BA_IM_RDX && 
							(ctr->usedRegisters & BA_CTRREG_RDX)));

					if (isPushOpResReg) {
						ba_AddIM(ctr, 2, BA_IM_PUSH, opResultReg);
						ctr->imStackSize += 8;
					}

					u64 oprRegAdj = ba_AdjRegSize(opResultReg, lhsSize);
					if (lhs->lexemeType == BA_TK_IDENTIFIER) {
						ba_AddIM(ctr, 5, BA_IM_MOV, oprRegAdj, BA_IM_ADRADD, 
							ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
							ba_CalcSTValOffset(ctr->currScope, lhs->val));
					}
					// (DPTR)
					else if (lhs->lexemeType == BA_TK_IMREGISTER) {
						ba_AddIM(ctr, 4, BA_IM_MOV, oprRegAdj, 
							BA_IM_ADR, (u64)lhs->val);
					}
					else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
						u64 adrLocReg = 
							realReg == BA_IM_RAX ? BA_IM_RCX : BA_IM_RAX;
						ba_AddIM(ctr, 2, BA_IM_PUSH, adrLocReg);
						ba_AddIM(ctr, 5, BA_IM_MOV, adrLocReg, BA_IM_ADRSUB, 
							BA_IM_RBP, (u64)lhs->val);
						ba_AddIM(ctr, 4, BA_IM_MOV, opResultReg, 
							BA_IM_ADR, adrLocReg);
						ba_AddIM(ctr, 2, BA_IM_POP, adrLocReg);
					}

					if (opLex == BA_TK_LSHIFTEQ || opLex == BA_TK_RSHIFTEQ) {
						if (realReg != BA_IM_RCX) {
							if (ctr->usedRegisters & BA_CTRREG_RCX) {
								ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
								ctr->imStackSize += 8;
							}
							ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, realReg);
						}

						ba_AddIM(ctr, 3, imOp, opResultReg, BA_IM_CL);

						if (realReg != BA_IM_RCX && 
							(ctr->usedRegisters & BA_CTRREG_RCX)) 
						{
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
							ctr->imStackSize -= 8;
						}
					}
					else {
						ba_AddIM(ctr, 3, imOp, opResultReg, realReg);
					}

					ba_AddIM(ctr, 3, BA_IM_MOV, realReg, opResultReg);

					if (isPushOpResReg) {
						ba_AddIM(ctr, 2, BA_IM_POP, opResultReg);
						ctr->imStackSize -= 8;
					}
				}

				if (lhs->lexemeType == BA_TK_IDENTIFIER) {
					ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, 
						ctr->imStackSize ? BA_IM_RBP : BA_IM_RSP, 
						ba_CalcSTValOffset(ctr->currScope, lhs->val), 
						ba_AdjRegSize(realReg, lhsSize));
				}
				// (DPTR)
				else if (lhs->lexemeType == BA_TK_IMREGISTER) {
					ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, 
						(u64)lhs->val, ba_AdjRegSize(realReg, lhsSize));
				}
				else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
					u64 adrLocReg = 
						realReg == BA_IM_RAX ? BA_IM_RCX : BA_IM_RAX;
					ba_AddIM(ctr, 2, BA_IM_PUSH, adrLocReg);
					ba_AddIM(ctr, 5, BA_IM_MOV, adrLocReg, BA_IM_ADRSUB, 
						BA_IM_RBP, (u64)lhs->val);
					ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, adrLocReg,
						realReg);
					ba_AddIM(ctr, 2, BA_IM_POP, adrLocReg);
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

				arg->typeInfo.type = lhsType.type;
				arg->isLValue = 0;
				ba_StkPush(ctr->pTkStk, arg);
				return 1;
			}

			// Comparison
			else if (ba_IsLexemeCompare(op->lexemeType)) {
				if (!ba_IsTypeNumeric(lhs->typeInfo.type) ||
					!ba_IsTypeNumeric(rhs->typeInfo.type))
				{
					return ba_ExitMsg(BA_EXIT_ERR, "comparison operation "
						"with non numeric operand(s) on", op->line, op->col,
						ctr->currPath);
				}

				if (ba_IsTypeUnsigned(lhs->typeInfo.type) ^ 
					ba_IsTypeUnsigned(rhs->typeInfo.type))
				{
					ba_WarnImplicitSignedConversion(op->line, op->col, 
						ctr->currPath, "comparison");
				}

				struct ba_PTkStkItem* rhsCopy = malloc(sizeof(*rhsCopy));
				memcpy(rhsCopy, rhs, sizeof(*rhsCopy));

				u64 opLex = op->lexemeType;
				u64 imOpSet = 0;
				u64 imOpJcc = 0;

				bool areBothUnsigned = ba_IsTypeUnsigned(lhs->typeInfo.type) && 
					ba_IsTypeUnsigned(rhs->typeInfo.type);

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

				u64 lhsStackPos = 0;
				u64 regL = (u64)lhs->val; // Kept only if lhs is a register
				u64 regR = (u64)rhs->val; // Kept only if rhs is a register

				ba_POpAsgnRegOrStack(ctr, lhs->lexemeType, &regL, &lhsStackPos);
				ba_POpAsgnRegOrStack(ctr, rhs->lexemeType, &regR, 0);
				
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

				ba_POpMovArgToReg(ctr, lhs, realRegL, isLhsLiteral);
				ba_POpMovArgToReg(ctr, rhs, realRegR, isRhsLiteral);

				if (lhs->lexemeType == BA_TK_IMREGISTER) {
					u64 lhsSize = ba_GetSizeOfType(lhs->typeInfo);
					if (lhsSize < 8) {
						ba_AddIM(ctr, 3, BA_IM_MOVZX, realRegL, 
							ba_AdjRegSize(realRegL, lhsSize));
					}
				}
				if (rhs->lexemeType == BA_TK_IMREGISTER) {
					u64 rhsSize = ba_GetSizeOfType(rhs->typeInfo);
					if (rhsSize < 8) {
						ba_AddIM(ctr, 3, BA_IM_MOVZX, realRegR, 
							ba_AdjRegSize(realRegR, rhsSize));
					}
				}

				u64 movReg = (u64)ba_StkTop(ctr->cmpRegStk);
				!movReg && (movReg = realRegL);

				ba_AddIM(ctr, 3, BA_IM_CMP, realRegL, realRegR);
				ba_AddIM(ctr, 2, imOpSet, movReg - BA_IM_RAX + BA_IM_AL);
				ba_AddIM(ctr, 3, BA_IM_MOVZX, movReg, 
					movReg - BA_IM_RAX + BA_IM_AL);

				arg->typeInfo.type = BA_TYPE_BOOL;
				arg->isLValue = 0;

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
						ctr->usedRegisters &= ~ba_IMToCtrReg(regL);
					}

					ba_StkPush(ctr->pTkStk, rhsCopy);
					ba_AddIM(ctr, 2, imOpJcc, (u64)ba_StkTop(ctr->cmpLblStk));
				}
				else {
					if (!ba_StkTop(ctr->cmpRegStk)) {
						ba_StkPush(ctr->pTkStk, arg);
					}

					if (regR) {
						ctr->usedRegisters &= ~ba_IMToCtrReg(regR);
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
			if (op->lexemeType == ')' || op->lexemeType == ']') {
				return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", op->line, 
					op->col, ctr->currPath);
			}
			// Func call
			else if (op->lexemeType == '(') {
				u64 funcArgsCnt = (u64)arg - 1;

				struct ba_PTkStkItem* funcTk = 
					ctr->pTkStk->items[ctr->pTkStk->count-(u64)arg];
				if (!funcTk || funcTk->typeInfo.type != BA_TYPE_FUNC) {
					return ba_ExitMsg(BA_EXIT_ERR, "attempt to call "
						"non-func on", op->line, op->col, ctr->currPath);
				}

				struct ba_Func* func = 
					((struct ba_STVal*)funcTk->val)->type.extraInfo;
				func->isCalled = 1;

				if (funcArgsCnt < func->paramCnt) {
					struct ba_FuncParam* param = func->firstParam;
					u64 paramIncrementer = funcArgsCnt;
					while (paramIncrementer) {
						if (!param) {
							break;
						}
						param = param->next;
						--paramIncrementer;
					}
					while (param && param->hasDefaultVal) {
						ba_StkPush(ctr->pTkStk, (void*)0);
						++funcArgsCnt;
						param = param->next;
					}
				}
				
				struct ba_Stk* argsStk = ba_NewStk();
				for (u64 i = 0; i < funcArgsCnt; i++) {
					struct ba_PTkStkItem* argItem = ba_StkPop(ctr->pTkStk);
					argItem && !ba_PCorrectDPtr(ctr, argItem) &&
						ba_ErrorDerefInvalid(op->line, op->col, ctr->currPath);
					ba_StkPush(argsStk, argItem);
				}

				ba_StkPop(ctr->pTkStk); // Pop funcTk
				
				if (!((struct ba_STVal*)funcTk->val)->isInited) {
					return ba_ExitMsg(BA_EXIT_ERR, "calling forward declared "
						"func that has not been given a definition on", 
						op->line, op->col, ctr->currPath);
				}

				if (funcArgsCnt != func->paramCnt) {
					fprintf(stderr, "Error: func on line %llu:%llu in %s "
						"takes %llu parameter%s, but %llu argument%s passed\n", 
						op->line, op->col, ctr->currPath, func->paramCnt, 
						func->paramCnt == 1 ? "" : "s", 
						funcArgsCnt, funcArgsCnt == 1 ? " was" : "s were");
					exit(-1);
				}
				
				if (!ctr->imStackSize) {
					ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
				}

				// Enter a new stack frame
				ba_UsedRegPreserve(ctr);
				ba_StkPush(ctr->funcFrameStk, (void*)ctr->usedRegisters);

				// Add arguments onto the stack
				struct ba_FuncParam* param = func->firstParam;
				u64 originalImStackSize = ctr->imStackSize;
				while (argsStk->count) {
					struct ba_PTkStkItem* funcArg = ba_StkPop(argsStk);
					u64 paramSize = ba_GetSizeOfType(param->type);
					if (funcArg) {
						bool isArgNum = ba_IsTypeNumeric(funcArg->typeInfo.type);
						bool isParamNum = ba_IsTypeNumeric(param->type.type);
						if ((isArgNum ^ isParamNum) || 
							(!isArgNum && !isParamNum && 
							!ba_AreTypesEqual(funcArg->typeInfo, param->type)))
						{
							fprintf(stderr, "Error: argument passed to func on "
								"line %llu:%llu in %s has invalid type (%s) "
								"for parameter of type %s\n", op->line, op->col,
								ctr->currPath,
								ba_GetTypeStr(funcArg->typeInfo), 
								ba_GetTypeStr(param->type));
							exit(-1);
						}
						
						u64 reg = (u64)funcArg->val;
						if (funcArg->lexemeType != BA_TK_IMREGISTER) {
							reg = ba_NextIMRegister(ctr);
						}

						if (!reg) {
							ba_POpFuncCallPushArgReg(ctr, BA_IM_RAX, paramSize);
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ctr->imStackSize += paramSize + 8;
						}
						
						ba_POpMovArgToReg(ctr, funcArg, reg, 
							ba_IsLexemeLiteral(funcArg->lexemeType));

						if (reg) {
							ba_POpFuncCallPushArgReg(ctr, reg, paramSize);
							if (paramSize < 8) {
								ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADR, 
									BA_IM_RSP, ba_AdjRegSize(reg, paramSize));
							}
							ctr->imStackSize += paramSize;
						}
						else {
							ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RSP, 
								0x8, ba_AdjRegSize(BA_IM_RAX, paramSize));
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
							ctr->imStackSize -= 8;
						}
					}
					// Default param
					else {
						if (!param->hasDefaultVal) {
							return ba_ExitMsg2(BA_EXIT_ERR, "func called on", 
								op->line, op->col, ctr->currPath, "with implicit "
								"argument for parameter that has no default");
						}
						u64 reg = ba_NextIMRegister(ctr);
						if (reg) {
							ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_IMM, 
								(u64)param->defaultVal);
							ba_POpFuncCallPushArgReg(ctr, reg, paramSize);
							if (paramSize < 8) {
								ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADR, 
									BA_IM_RSP, ba_AdjRegSize(reg, paramSize));
							}
						}
						else {
							ba_POpFuncCallPushArgReg(ctr, BA_IM_RAX, paramSize);
							ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
							ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 
								paramSize < 8 
									? (u64)param->defaultVal & 
										((1llu << (paramSize*8))-1)
									: (u64)arg->val);
							ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RSP, 
								0x8, ba_AdjRegSize(BA_IM_RAX, paramSize));
							ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						}
						ctr->imStackSize += paramSize;
					}

					param = param->next;
				}

				// Reset stack
				ba_DelStk(argsStk);
				ctr->imStackSize = originalImStackSize;

				// Call the function, clear args from stack, init return value
				ba_AddIM(ctr, 2, BA_IM_LABELCALL, func->lblStart);
				if (func->paramStackSize) {
					ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, 
						BA_IM_IMM, func->paramStackSize);
				}
				struct ba_PTkStkItem* retVal = malloc(sizeof(*retVal));
				if (!retVal) {
					return ba_ErrorMallocNoMem();
				}

				// TODO: get return value from stack
				
				// Leave the stack frame
				ctr->usedRegisters = (u64)ba_StkPop(ctr->funcFrameStk);

				// Get return value from rax
				u64 regRet = BA_IM_RAX;
				u64 stackPos = 0;
				if (ctr->usedRegisters & BA_CTRREG_RAX) {
					ba_POpAsgnRegOrStack(ctr, /* lexType = */ 0, 
						&regRet, &stackPos);
					ctr->usedRegisters &= ~ba_IMToCtrReg(regRet);
					if (regRet) {
						// Restore every register except RAX
						ba_UsedRegRestore(ctr, 1);
						ba_AddIM(ctr, 3, BA_IM_MOV, regRet, BA_IM_RAX);
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
						ctr->imStackSize -= 8;
					}
					else {
						// Restore every register except the last two 
						// (should be RCX, RAX)
						ba_UsedRegRestore(ctr, 2);
						// Swap the return value and the original rax value
						ba_AddIM(ctr, 2, BA_IM_MOV, BA_IM_RCX, BA_IM_RAX);
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, BA_IM_ADRADD, 
							BA_IM_RSP, 0x8);
						ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RSP, 
							0x8, BA_IM_RCX);
						ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
						ctr->imStackSize += 8;
					}
					ctr->usedRegisters |= ba_IMToCtrReg(regRet);
				}
				else {
					ba_UsedRegRestore(ctr, 0);
					ctr->usedRegisters |= BA_CTRREG_RAX;
				}

				ba_POpSetArg(ctr, retVal, regRet, stackPos);
				
				retVal->typeInfo = func->retType;
				retVal->isLValue = 0;
				ba_StkPush(ctr->pTkStk, retVal);

				return 2; // Go back to parsing as if having followed an atom
			}
			else if (op->lexemeType == '~') {
				!ba_PCorrectDPtr(ctr, arg) && 
					ba_ErrorDerefInvalid(op->line, op->col, ctr->currPath);
				struct ba_PTkStkItem* castedExp = ba_StkPop(ctr->pTkStk);
				if (!castedExp) {
					return ba_ExitMsg(BA_EXIT_ERR, "syntax error on", 
						op->line, op->col, ctr->currPath);
				}
				struct ba_PTkStkItem* typeArg = arg;
				struct ba_Type newType = 
					*(struct ba_Type*)typeArg->typeInfo.extraInfo;

				bool isNewTypeNumeric = ba_IsTypeNumeric(newType.type);
				bool isOldTypeNumeric = 
					ba_IsTypeNumeric(castedExp->typeInfo.type);

				if (!newType.type || newType.type == BA_TYPE_VOID) {
					return ba_ExitMsg(BA_EXIT_ERR, "cast to expression that "
						"is not a (castable) type on", op->line, op->col, 
						ctr->currPath);
				}

				if (castedExp->typeInfo.type == BA_TYPE_ARR) {
					if (newType.type != BA_TYPE_ARR || 
						(ba_GetSizeOfType(castedExp->typeInfo) != 
							ba_GetSizeOfType(newType)))
					{
						return ba_ErrorCastTypes(op->line, op->col, 
							ctr->currPath, castedExp->typeInfo, newType);
					}
				}
				else if ((isNewTypeNumeric != isOldTypeNumeric) || 
					(!isNewTypeNumeric && !isOldTypeNumeric)) 
				{
					return ba_ErrorCastTypes(op->line, op->col, 
						ctr->currPath, castedExp->typeInfo, newType);
				}

				arg = castedExp;
				arg->typeInfo = newType;
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
