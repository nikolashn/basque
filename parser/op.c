// See LICENSE for copyright/license information

#include "common.h"

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
				op->lexemeType == BA_TK_KW_LENGTHOF || 
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
void ba_POpAsgnRegOrStack(struct ba_Ctr* ctr, u64 lexType, u64* reg, 
	u64* stackPos) 
{
	// *reg == 0 means on the stack, which is handled outside of this func
	(lexType != BA_TK_IMREGISTER) && (*reg = ba_NextIMRegister(ctr));

	if (!*reg) {
		stackPos && (*stackPos = ctr->imStackSize + 8);
		// First: result location, second: preserve rax
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
		ctr->imStackSize += 16;
	}
}

void ba_POpMovIdToReg(struct ba_Ctr* ctr, struct ba_STVal* id, u64 argSize, 
	u64 reg, bool isLea)
{
	bool isPopRbp = 0;
	i64 offset = ba_CalcVarOffset(ctr, id, &isPopRbp);
	ba_AddIM(ctr, 5, isLea ? BA_IM_LEA : BA_IM_MOV, 
		ba_AdjRegSize(reg, argSize), 
		offset < 0 ? BA_IM_ADRSUB : BA_IM_ADRADD, BA_IM_RBP, 
		offset < 0 ? -offset : offset);
	if (isPopRbp) {
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
	}
}

bool ba_POpMovArgToReg(struct ba_Ctr* ctr, struct ba_PTkStkItem* arg, u64 reg, 
	bool isLiteral) 
{
	u64 argSize = ba_GetSizeOfType(arg->typeInfo);

	// Arrays are resolved as pointers to their starting location
	bool isArr = arg->typeInfo.type == BA_TYPE_ARR;
	if (isArr) {
		argSize = ba_GetSizeOfType((struct ba_Type){ BA_TYPE_PTR, 0 });
	}

	if (arg->lexemeType == BA_TK_IDENTIFIER) {
		bool isSigned = ba_IsTypeSigned(arg->typeInfo);
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
		ba_AddIM(ctr, 5, isArr ? BA_IM_LEA : BA_IM_MOV, reg, BA_IM_ADRSUB, 
			BA_IM_RBP, ctr->currScope->dataSize + (u64)arg->val);
		return 1;
	}
	if (isLiteral) {
		if (isArr) {
			return 0;
		}
		u64 val = (argSize < 8) ? (u64)arg->val & ((1llu << (argSize*8))-1)
			: (u64)arg->val;
		ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_IMM, val);
		return 1;
	}
	return 0;
}

bool ba_POpMovArgToRegDPTR(struct ba_Ctr* ctr, struct ba_PTkStkItem* arg,
	u64 size, u64 reg, u64 testReg, u64 tmpRegDef, u64 tmpRegBackup) 
{
	if (arg->lexemeType == BA_TK_IDENTIFIER) {
		ba_POpMovIdToReg(ctr, arg->val, size, reg, /* isLea = */ 0);
		return 1;
	}
	if (arg->lexemeType == BA_TK_IMREGISTER) { // DPTR
		u64 rszdReg = ba_AdjRegSize(reg, size);
		ba_AddIM(ctr, 4, BA_IM_MOV, rszdReg, BA_IM_ADR, (u64)arg->val);
		return 1;
	}
	if (arg->lexemeType == BA_TK_IMRBPSUB) { // DPTR
		u64 tmpReg = testReg == tmpRegDef ? tmpRegBackup : tmpRegDef;
		ba_AddIM(ctr, 2, BA_IM_PUSH, tmpReg);
		ba_AddIM(ctr, 5, BA_IM_MOV, tmpReg, BA_IM_ADRSUB, BA_IM_RBP, 
			ctr->currScope->dataSize + (u64)arg->val);
		ba_AddIM(ctr, 4, BA_IM_MOV, reg, BA_IM_ADR, tmpReg);
		ba_AddIM(ctr, 2, BA_IM_POP, tmpReg);
		return 1;
	}
	return 0;
}

void ba_POpSetArg(struct ba_Ctr* ctr, struct ba_PTkStkItem* arg, u64 reg, 
	u64 stackPos) 
{
	if (reg) {
		arg->lexemeType = BA_TK_IMREGISTER;
		arg->val = (void*)reg;
	}
	else {
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
			ctr->currScope->dataSize + stackPos, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;
		arg->lexemeType = BA_TK_IMRBPSUB;
		arg->val = (void*)ctr->imStackSize;
	}
}

// Handle unary operators with a non literal operand
void ba_POpNonLitUnary(u64 opLexType, struct ba_PTkStkItem* arg,
	struct ba_Ctr* ctr)
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
void ba_POpNonLitBinary(struct ba_Ctr* ctr, u64 imOp, struct ba_PTkStkItem* lhs,
	struct ba_PTkStkItem* rhs, struct ba_PTkStkItem* arg, bool isLhsLiteral, 
	bool isRhsLiteral, bool isShortCirc)
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
void ba_POpNonLitBitShift(struct ba_Ctr* ctr, u64 imOp, 
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
u8 ba_PCorrectDPtr(struct ba_Ctr* ctr, struct ba_PTkStkItem* item) {
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
			BA_IM_RBP, ctr->currScope->dataSize + (u64)item->val);
		ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP, 
			ctr->currScope->dataSize + (u64)item->val, BA_IM_RAX);
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
	}
	return 1;
}

u8 ba_POpAssignChecks(struct ba_Ctr* ctr, struct ba_Type lhsType, 
	struct ba_PTkStkItem* rhs, u64 line, u64 col) 
{
	if (lhsType.type == BA_TYPE_FUNC) {
		return ba_ExitMsg(BA_EXIT_ERR, "cannot assign func directly to a "
			"value,", line, col, ctr->currPath);
	}
	else if ((lhsType.type == BA_TYPE_ARR && 
			rhs->typeInfo.type == BA_TYPE_ARR && 
			ba_GetSizeOfType(lhsType) == ba_GetSizeOfType(rhs->typeInfo)) || 
		(lhsType.type == BA_TYPE_ARR && !ba_GetSizeOfType(lhsType)))
	{
		return 1;
	}
	else if (ba_IsTypeNum(lhsType) && ba_IsTypeNum(rhs->typeInfo)) {
		if (lhsType.type == BA_TYPE_PTR) {
			// 0 for null pointer is fine
			if (ba_IsLexemeLiteral(rhs->lexemeType) && (u64)rhs->val == 0) {
				return 1;
			}
			if (rhs->typeInfo.type != BA_TYPE_PTR) {
				ba_ExitMsg(BA_EXIT_WARN, "assignment of numeric non-pointer "
					"to pointer on", line, col, ctr->currPath);
			}
			else if (!ba_IsPermitConvertPtr(lhsType, rhs->typeInfo)) {
				return ba_ExitMsg(BA_EXIT_WARN, "assignment of pointer to non-void " 
					"pointer of different type on", line, col, ctr->currPath);
			}
		}
		return 1;
	}
	else if (ba_AreTypesEqual(lhsType, rhs->typeInfo)) {
		return 1;
	}
	return ba_ErrorConvertTypes(line, col, ctr->currPath, rhs->typeInfo, lhsType);
}

void ba_POpFuncCallPushArgReg(struct ba_Ctr* ctr, u64 reg, u64 size) {
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

u64 ba_AllocStrLitStatic(struct ba_Ctr* ctr, struct ba_Str* str) {
	u64 staticStart = ctr->staticSeg->cnt;
	ctr->staticSeg->cnt += str->len + 1;
	(ctr->staticSeg->cnt > ctr->staticSeg->cap) && 
		ba_ResizeDynArr8(ctr->staticSeg);
	u8* memStart = ctr->staticSeg->arr + staticStart;
	memcpy(memStart, str->str, str->len + 1);
	str->staticStart = staticStart;
	return staticStart;
}

void ba_PAssignArr(struct ba_Ctr* ctr, struct ba_PTkStkItem* destItem, 
	struct ba_PTkStkItem* srcItem, u64 size)
{
	{ // Recognize MemCopy as having been called
		struct ba_STVal* stVal = ba_HTGet(ctr->globalST->ht, "MemCopy");
		((struct ba_Func*)stVal->type.extraInfo)->isCalled = 1;
	}

	// Setting the default register so that it does not trample on used ones
	u64 defaultReg = BA_IM_RAX;
	u64 d = 0;
	destItem->lexemeType == BA_TK_IMREGISTER && (d = (u64)destItem->val);
	u64 s = 0;
	srcItem->lexemeType == BA_TK_IMREGISTER && (s = (u64)srcItem->val);
	if (d == BA_IM_RAX || s == BA_IM_RAX) {
		defaultReg = (d == BA_IM_RCX || s == BA_IM_RCX) ? BA_IM_RDX : BA_IM_RCX;
	}
	u64 reg = defaultReg;

	if (srcItem->lexemeType == BA_TK_IMREGISTER && !destItem->lexemeType && 
		srcItem->val == (void*)BA_IM_RSP && destItem->val == (void*)BA_IM_RSP)
	{
		return;
	}

	// Destination pointer
	if (!destItem->lexemeType) {
		reg = (u64)destItem->val;
	}
	else if (destItem->lexemeType == BA_TK_IDENTIFIER) {
		ba_POpMovIdToReg(ctr, destItem->val, 8, defaultReg, /* isLea = */ 1);
	}
	else if (destItem->lexemeType == BA_TK_IMREGISTER) {
		// (DPTR)
		ba_AddIM(ctr, 4, BA_IM_MOV, defaultReg, BA_IM_ADR, (u64)destItem->val);
	}
	else if (destItem->lexemeType == BA_TK_IMRBPSUB) {
		// (DPTR)
		ba_AddIM(ctr, 5, BA_IM_MOV, defaultReg, BA_IM_ADRSUB, BA_IM_RBP, 
			ctr->currScope->dataSize + (u64)destItem->val);
	}
	ba_AddIM(ctr, 3, BA_IM_PUSH, reg); // dest ptr
	ctr->imStackSize += 8;

	// Source pointer
	reg = defaultReg;
	if (srcItem->lexemeType == BA_TK_IDENTIFIER) {
		ba_POpMovIdToReg(ctr, srcItem->val, 8, defaultReg, /* isLea = */ 1);
	}
	else if (srcItem->lexemeType == BA_TK_IMREGISTER) {
		reg = (u64)srcItem->val;
	}
	else if (srcItem->lexemeType == BA_TK_IMSTATIC) {
		// Non-constant array literals
		ba_AddIM(ctr, 4, BA_IM_MOV, defaultReg, BA_IM_STATIC, (u64)srcItem->val);
	}
	else if (srcItem->lexemeType == BA_TK_LITSTR) {
		ba_AddIM(ctr, 4, BA_IM_MOV, defaultReg, BA_IM_STATIC, 
			ba_AllocStrLitStatic(ctr, (struct ba_Str*)srcItem->val));
	}
	else if (srcItem->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(ctr, 5, BA_IM_LEA, defaultReg, BA_IM_ADRSUB, BA_IM_RBP, 
			ctr->currScope->dataSize + (u64)srcItem->val);
	}
	ba_AddIM(ctr, 2, BA_IM_PUSH, reg); // src ptr
	ctr->imStackSize -= 8; // so that memory is not automatically deallocated
	
	ba_AddIM(ctr, 4, BA_IM_MOV, defaultReg, BA_IM_IMM, size); // mem size
	ba_AddIM(ctr, 2, BA_IM_PUSH, defaultReg);
	ba_AddIM(ctr, 2, BA_IM_LABELCALL, ba_BltinLblGet(BA_BLTIN_CoreMemCopy));
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RSP, BA_IM_IMM, 0x18);
}

/* A helper used to stand in for duplicated code in division and modulo
 * operations as well as their assignment counterparts.
 * For assignment: regL == regR (only 1 temporary register is needed),
 * For division: realReg == 0 (this allows it to be automatically calculated) */
void ba_POpNonLitDivMod(struct ba_Ctr* ctr, struct ba_PTkStkItem* lhs, 
	struct ba_PTkStkItem* rhs, struct ba_PTkStkItem* arg, u64 regL, u64 regR,
	u64 realReg, u64 lhsStackPos, struct ba_Type lhsType, 
	bool isDiv, bool isAssign)
{
	bool areBothUnsigned = ba_IsTypeUnsigned(lhsType) &&
		ba_IsTypeUnsigned(rhs->typeInfo);

	// [0] = rax, [1] = rdx
	bool isPushedFlags[2] = { 0, 0 };
	u64 tmpCtrRegs[2] = { BA_CTRREG_RAX, BA_CTRREG_RDX };
	u64 tmpRegs[2] = { BA_IM_RAX, BA_IM_RDX };

	for (u64 i = 0; i < 2; ++i) {
		if ((ctr->usedRegisters & tmpCtrRegs[i]) && regL != tmpRegs[i]) {
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

	if (isAssign) { // Division/modulo assign needs to account for DPTRs
		ba_POpMovArgToRegDPTR(ctr, lhs, ba_GetSizeOfType(lhs->typeInfo), 
			BA_IM_RAX, realReg, BA_IM_RDX, BA_IM_RCX);
	}
	else { // Normal divison/modulo operation
		if (regR) {
			ctr->usedRegisters &= ~ba_IMToCtrReg(regR);
			realReg = regR;
		}
		else {
			ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RCX);
			ctr->imStackSize += 8;
			realReg = BA_IM_RCX;
		}
		
		ba_POpMovArgToReg(ctr, lhs, BA_IM_RAX, 
			ba_IsLexemeLiteral(lhs->lexemeType));

		if (isDiv && lhs->lexemeType == BA_TK_IMREGISTER && 
			(u64)lhs->val != BA_IM_RAX) 
		{
			ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, (u64)lhs->val);
		}

		ba_POpMovArgToReg(ctr, rhs, realReg, 
			ba_IsLexemeLiteral(rhs->lexemeType));
	}

	ba_AddIM(ctr, 1, BA_IM_CQO);
	ba_AddIM(ctr, 2, areBothUnsigned ? BA_IM_DIV : BA_IM_IDIV, realReg);

	if (isDiv) {
		ba_AddIM(ctr, 3, BA_IM_MOV, isAssign ? realReg : regL, BA_IM_RAX);
		if (!isAssign) {
			ba_POpSetArg(ctr, arg, regL, lhsStackPos);
		}
	}
	else {
		if (!areBothUnsigned) {
			/* Test result for sign: if negative, then operands 
			 * had opposite signs and so the rhs needs to be 
			 * added */
			u64 raxAdj = ba_AdjRegSize(BA_IM_RAX, 
				ba_GetSizeOfType(rhs->typeInfo));
			ba_AddIM(ctr, 3, BA_IM_TEST, raxAdj, raxAdj);
			ba_AddIM(ctr, 2, BA_IM_SETS, BA_IM_AL);
			ba_AddIM(ctr, 3, BA_IM_MOVZX, BA_IM_RAX, BA_IM_AL);
			ba_AddIM(ctr, 3, BA_IM_IMUL, BA_IM_RAX, realReg);
			ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RDX, BA_IM_RAX);
		}

		if (isAssign) {
			ba_AddIM(ctr, 3, BA_IM_MOV, realReg, BA_IM_RDX);
		}
		else {
			if (regL) {
				if (regL != BA_IM_RDX) {
					ba_AddIM(ctr, 3, BA_IM_MOV, regL, BA_IM_RDX);
				}
				arg->lexemeType = BA_TK_IMREGISTER;
				arg->val = (void*)regL;
			}
			else {
				ba_AddIM(ctr, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
					ctr->currScope->dataSize + lhsStackPos, BA_IM_RDX);
				arg->lexemeType = BA_TK_IMRBPSUB;
				arg->val = (void*)ctr->imStackSize;
			}
		}
	}
	
	// Restore registers
	if (isPushedFlags[1]) {
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
		ctr->imStackSize -= 8;
	}
	if (!isAssign) {
		if (lhs->lexemeType == BA_TK_IMREGISTER) {
			ctr->usedRegisters &= ~BA_CTRREG_RDX;
		}
		if (!regR) {
			ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RCX);
			ctr->imStackSize -= 8;
		}
	}
	if (isPushedFlags[0]) {
		ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RAX);
		ctr->imStackSize -= 8;
	}
}

