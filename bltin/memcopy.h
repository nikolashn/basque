// See LICENSE for copyright/license information

#ifndef BA__BLTIN_MemCopy_H
#define BA__BLTIN_MemCopy_H

#include "bltinflags.h"

// TODO: page copy, alignment

/* Params: dest ptr (0x8), src ptr (0x8), mem size (0x8)
 * Returns nothing */
void ba_BltinMemCopy(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_MemCopy);
	ba_BltinLabels[BA_BLTIN_MemCopy] = ctr->labelCnt;
	ctr->labelCnt += 5;
	
	struct ba_IM* oldIM = ctr->im;
	struct ba_IM* oldStartIM = ctr->startIM;

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	// --- MemCopy ---
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5);
	// Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX);
	// Preserve rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	// rax: dest ptr
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RAX, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	// rcx: src ptr
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RCX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	// rdx: mem size
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
	// Restore rbp
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);
	// Push return location
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX);

	ctr->im->vals = malloc(sizeof(u64));
	if (!ctr->im->vals) {
		ba_ErrorMallocNoMem();
	}
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = oldStartIM;
	ctr->im = oldIM;
}

#endif
