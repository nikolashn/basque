// See LICENSE for copyright/license information

#ifndef BA__PARSER_OP_H
#define BA__PARSER_OP_H

#include "lexer.h"
#include "bltin/bltin.h"

/* NOTE: These functions are intended to be called by multiple different 
 * operators and so should not contain any (source code) operator specific 
 * exit messages.
 */

// Handle unary operators with a non literal operand
void ba_POpNonLitUnary(u64 opLexType, struct ba_PTkStkItem* arg,
	struct ba_Controller* ctr)
{
	u64 imOp = 0;
	((opLexType == '-') && (imOp = BA_IM_NEG)) ||
	((opLexType == '~') && (imOp = BA_IM_NOT)) ||
	((opLexType == '!') && (imOp = 1));

	if (!imOp) {
		printf("Error: Unary lexeme type %llx passed to ba_POpNonLitUnary "
			"not recognized.\n", opLexType);
		exit(-1);
	}

	u64 stackPos = 0;
	u64 reg = (u64)arg->val; // Kept only if arg is a register

	if (arg->lexemeType != BA_TK_IMREGISTER) {
		reg = ba_NextIMRegister(ctr);
	}

	if (!reg) {
		if (!ctr->imStackCnt) {
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, 
				BA_IM_RSP);
		}
		++ctr->imStackCnt;
		ctr->imStackSize += 8;
		stackPos = ctr->imStackSize;
		// First: result location, second: preserve rax
		ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	}

	u64 realReg = reg ? reg : BA_IM_RAX;

	if (arg->lexemeType == BA_TK_IDENTIFIER) {
		ba_AddIM(&ctr->im, 4, BA_IM_MOV, realReg, 
			BA_IM_DATASGMT, 
			((struct ba_STVal*)arg->val)->address);
	}
	else if (arg->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(&ctr->im, 5, BA_IM_MOV, realReg, 
			BA_IM_ADRSUB, BA_IM_RBP, (u64)arg->val);
	}

	if (opLexType == '!') {
		ba_AddIM(&ctr->im, 3, BA_IM_TEST, realReg, realReg);
		ba_AddIM(&ctr->im, 2, BA_IM_SETZ, 
			realReg - BA_IM_RAX + BA_IM_AL);
		ba_AddIM(&ctr->im, 3, BA_IM_MOVZX, realReg, 
			realReg - BA_IM_RAX + BA_IM_AL);
	}
	else {
		ba_AddIM(&ctr->im, 2, imOp, realReg);
	}

	if (reg) {
		arg->lexemeType = BA_TK_IMREGISTER;
		arg->val = (void*)reg;
	}
	else {
		ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, 
			BA_IM_RBP, stackPos, BA_IM_RAX);
		ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);

		arg->lexemeType = BA_TK_IMRBPSUB;
		arg->val = (void*)ctr->imStackSize;
	}
}

// Handle binary operators with a non literal operand
void ba_POpNonLitBinary(u64 imOp, struct ba_PTkStkItem* arg,
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	u8 isLhsLiteral, u8 isRhsLiteral, u8 isShortCirc, struct ba_Controller* ctr)
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

	ba_AddIM(&ctr->im, 3, imOp, regL ? regL : BA_IM_RAX, 
		regR ? regR : rhsReplacement);

	if (isShortCirc) {
		u64 realRegL = regL ? regL : BA_IM_RAX;
		ba_AddIM(&ctr->im, 2, BA_IM_LABEL, ba_StkPop(ctr->shortCircLblStk));
		ba_AddIM(&ctr->im, 3, BA_IM_TEST, realRegL, realRegL);
		ba_AddIM(&ctr->im, 2, BA_IM_SETNZ, realRegL - BA_IM_RAX + BA_IM_AL);
		ba_AddIM(&ctr->im, 3, BA_IM_MOVZX, realRegL,
			realRegL - BA_IM_RAX + BA_IM_AL);
	}

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

// Handle bit shifts with at least one non literal operand
void ba_POpNonLitBitShift(u64 imOp, struct ba_PTkStkItem* arg,
	struct ba_PTkStkItem* lhs, struct ba_PTkStkItem* rhs, 
	u8 isRhsLiteral, struct ba_Controller* ctr) 
{
	u8 isLhsOriginallyRcx = 0;
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
		if (!ctr->imStackCnt) {
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
		}
		++ctr->imStackCnt;
		ctr->imStackSize += 8;
		lhsStackPos = ctr->imStackSize;
		// First: result location, second: preserve rax
		ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
		ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	}

	if (lhs->lexemeType == BA_TK_IDENTIFIER) {
		ba_AddIM(&ctr->im, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX, 
			BA_IM_DATASGMT, ((struct ba_STVal*)lhs->val)->address);
	}
	else if (lhs->lexemeType == BA_TK_IMRBPSUB) {
		ba_AddIM(&ctr->im, 5, BA_IM_MOV, regL ? regL : BA_IM_RAX,
			BA_IM_ADRSUB, BA_IM_RBP, (u64)lhs->val);
	}
	else if (lhs->lexemeType == BA_TK_IMREGISTER) {
		if (isLhsOriginallyRcx) {
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, 
				regL ? regL : BA_IM_RAX, BA_IM_RCX);
		}
	}
	else { // Literal (immediate) lhs
		ba_AddIM(&ctr->im, 4, BA_IM_MOV, regL ? regL : BA_IM_RAX, 
			BA_IM_IMM, (u64)lhs->val);
	}

	if (isRhsLiteral) {
		ba_AddIM(&ctr->im, 4, imOp, regL ? regL : BA_IM_RAX, 
			BA_IM_IMM, (u64)rhs->val & 0x3f);
	}
	else {
		u64 regTmp = BA_IM_RCX;
		u8 rhsIsRcx = rhs->lexemeType == BA_TK_IMREGISTER &&
			(u64)rhs->val == BA_IM_RCX;

		if ((ctr->usedRegisters & BA_CTRREG_RCX) && !rhsIsRcx) {
			regTmp = ba_NextIMRegister(ctr);
			if (!regTmp) {
				// Don't need to store rhs, only preserve rax
				ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
			}
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, 
				regTmp ? regTmp : BA_IM_RAX, BA_IM_RCX);
		}

		if (rhs->lexemeType == BA_TK_IDENTIFIER) {
			ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RCX, 
				BA_IM_DATASGMT,
				((struct ba_STVal*)rhs->val)->address);
		}
		else if (rhs->lexemeType == BA_TK_IMRBPSUB) {
			ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_RCX,
				BA_IM_ADRSUB, BA_IM_RBP, (u64)rhs->val);
		}
		else if (!rhsIsRcx) { // Register that isn't rcx
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RCX, 
				(u64)rhs->val);
			ctr->usedRegisters &= ~ba_IMToCtrRegister((u64)rhs->val);
		}

		// The actual shift operation
		ba_AddIM(&ctr->im, 3, imOp, regL ? regL : BA_IM_RAX, 
			BA_IM_CL);

		if ((ctr->usedRegisters & BA_CTRREG_RCX) && !rhsIsRcx) {
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RCX, 
				regTmp ? regTmp : BA_IM_RAX);
		}

		if (!regTmp) {
			ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
		}

		if (regTmp && regTmp != BA_IM_RCX) {
			ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RCX, regTmp);
			ctr->usedRegisters &= ~ba_IMToCtrRegister(regTmp);
		}
	}

	if (regL) {
		arg->lexemeType = BA_TK_IMREGISTER;
		arg->val = (void*)regL;
	}
	else {
		ba_AddIM(&ctr->im, 5, BA_IM_MOV, BA_IM_ADRSUB, BA_IM_RBP,
			lhsStackPos, BA_IM_RAX);
		ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RAX);
		arg->lexemeType = BA_TK_IMRBPSUB;
		arg->val = (void*)ctr->imStackSize;
	}
}

#endif
