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
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP); // Praeserve rbp
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	// rax: dest ptr, rcx: src ptr, rdx: mem size
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, BA_IM_ADRADD, BA_IM_RBP, 0x20);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RCX, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);

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
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* Params: ptr (0x8), byte (0x1), size (0x8)
 * Returns nothing */
void ba_BltinCoreMemSet(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_CoreMemSet);
	ba_BltinLblSet(BA_BLTIN_CoreMemSet, ctr->labelCnt);
	ctr->labelCnt += 5;
	
	// --- MemSet ---
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP); // Praeserve rbp
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	// rax: ptr, cl: byte, rdx: size
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RCX, BA_IM_RCX);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, BA_IM_ADRADD, BA_IM_RBP, 0x19);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_CL, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	// Fill rcx
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_SHL, BA_IM_RCX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 3, BA_IM_OR, BA_IM_RCX, BA_IM_RBX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_SHL, BA_IM_RCX, BA_IM_IMM, 16);
	ba_AddIM(ctr, 3, BA_IM_OR, BA_IM_RCX, BA_IM_RBX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_SHL, BA_IM_RCX, BA_IM_IMM, 32);
	ba_AddIM(ctr, 3, BA_IM_OR, BA_IM_RCX, BA_IM_RBX);

	// If whole words can be set, set whole words, otherwise set bytes
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4);
	ba_AddIM(ctr, 4, BA_IM_CMP, BA_IM_RDX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 2, BA_IM_LABELJAE, ctr->labelCnt-2);

	// Set bytes
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RAX, BA_IM_CL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RAX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-3);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1);

	// Set words
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RAX, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_RAX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RDX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-4);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1);

	// Epilogue
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 1, BA_IM_RET);
}
void ba_IncludeCore(struct ba_Ctr* ctr) {
	struct ba_Type voidPtr = { BA_TYPE_PTR, ba_MAlloc(sizeof(struct ba_Type)) };
	((struct ba_Type*)voidPtr.extraInfo)->type = BA_TYPE_VOID;
	
	if (!ba_BltinFlagsTest(BA_BLTIN_CoreMemCopy)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, 0, 0, "MemCopy");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinCoreMemCopy(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam(),
		};
		*(params[2]) = (struct ba_FuncParam){ // size
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
		};
		*(params[1]) = (struct ba_FuncParam){ // src
			.type = voidPtr,
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // dest
			.type = voidPtr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_VOID, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_CoreMemCopy);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_CoreMemSet)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, 0, 0, "MemSet");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinCoreMemSet(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam(),
		};
		*(params[2]) = (struct ba_FuncParam){ // size
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
		};
		*(params[1]) = (struct ba_FuncParam){ // byte
			.type = (struct ba_Type){ BA_TYPE_U8, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // ptr
			.type = voidPtr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_VOID, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_CoreMemSet);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x11;
		func->firstParam = params[0];
	}
}

