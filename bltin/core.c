// See LICENSE for copyright/license information

#include "bltin.h"

// TODO: page copy, alignment

/* Params: dest (0x8), src (0x8), size (0x8)
 * Returns nothing */
void ba_BltinCoreMemCopy(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_CoreMemCopy);
	ba_BltinLblSet(BA_BLTIN_CoreMemCopy, ctr->labelCnt);
	ctr->labelCnt += 5;
	
	// --- MemCopy ---
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP); // Praeserve rbp
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	// rax: dest ptr, rcx: src ptr, rdx: mem size
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RCX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x08);

	// If whole words can be copied, copy whole words, otherwise copy bytes
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4);
	ba_AddIM(ctr, 4, BA_IM_CMP, BA_IM_RDX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 2, BA_IM_LABELJAE, ctr->labelCnt-2);

	// Copy bytes
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_SIL, BA_IM_ADR, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RAX, BA_IM_SIL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RAX);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RCX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-3);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1);

	// Copy words
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RSI, BA_IM_ADR, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RAX, BA_IM_RSI);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RAX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RCX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RDX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-4);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1);

	// Epilogue
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

void ba_IncludeCore(struct ba_Ctr* ctr) {
	if (!ba_BltinFlagsTest(BA_BLTIN_CoreMemCopy)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, 0, 0, "MemCopy");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinCoreMemCopy(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_VOID, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_CoreMemCopy);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;

		struct ba_FuncParam* params[3];

		params[0] = ba_NewFuncParam(); // dest
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		params[1] = ba_NewFuncParam(); // src
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[1]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		params[2] = ba_NewFuncParam(); // size
		params[2]->type = (struct ba_Type){ BA_TYPE_U64, 0 };
		params[2]->hasDefaultVal = 1;
		params[2]->defaultVal = 0; // sys.FD_STDIN
		params[1]->next = params[2];
	}
}

