// See LICENSE for copyright/license information

#ifndef BA__BLTIN_U64ToStr_H
#define BA__BLTIN_U64ToStr_H

#include "bltinflags.h"

/* Params (rax): int to convert
 * Returns (rax): string length; (stack): string (0x30)
 */
void ba_BltinU64ToStr(struct ba_Controller* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_U64ToStr);
	ba_BltinLabels[BA_BLTIN_U64ToStr] = ctr->labelCnt;
	ctr->labelCnt += 5;
	
	struct ba_IM* oldIM = ctr->im;

	ctr->entryIM = ctr->startIM;
	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, ctr->labelCnt-5);
	ba_AddIM(&ctr->im, 2, BA_IM_POP, BA_IM_RBX);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	// Integer value
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_R8, BA_IM_RAX);
	// String length
	ba_AddIM(&ctr->im, 3, BA_IM_XOR, BA_IM_R9, BA_IM_R9);
	ba_AddIM(&ctr->im, 3, BA_IM_TEST, BA_IM_RAX, BA_IM_RAX);
	// In the case that the number is 0
	ba_AddIM(&ctr->im, 2, BA_IM_LABELJNZ, ctr->labelCnt-4);
	ba_AddIM(&ctr->im, 6, BA_IM_MOV, BA_IM_64ADRSUB, BA_IM_RBP, 0x30, BA_IM_IMM, 0x30);
	ba_AddIM(&ctr->im, 2, BA_IM_INC, BA_IM_R9);
	ba_AddIM(&ctr->im, 2, BA_IM_LABELJMP, ctr->labelCnt-2);

	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, ctr->labelCnt-4);
	ba_AddIM(&ctr->im, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 0x3030303030303030);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RAX);
	// Offset from the bottom of the stack of where the digits start
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_R10, BA_IM_IMM, 0x18);
	// Ensures that later test cl,cl doesn't use garbage data
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_CL, BA_IM_IMM, 1);

	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, ctr->labelCnt-3);
	ba_AddIM(&ctr->im, 3, BA_IM_TEST, BA_IM_CL, BA_IM_CL);
	// Subtract 8 from the offset every 8 digits
	ba_AddIM(&ctr->im, 2, BA_IM_LABELJNZ, ctr->labelCnt-2);
	ba_AddIM(&ctr->im, 4, BA_IM_SUB, BA_IM_R10, BA_IM_IMM, 0x08);
	
	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, ctr->labelCnt-2);
	// lastDigit = num % 10; num /= 10
	// uses magic to divide without using div instruction
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RCX, BA_IM_R8);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, 0xcccccccccccccccd);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RCX);
	ba_AddIM(&ctr->im, 2, BA_IM_MUL, BA_IM_RDX);
	ba_AddIM(&ctr->im, 4, BA_IM_SHR, BA_IM_RDX, BA_IM_IMM, 3);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(&ctr->im, 4, BA_IM_SHL, BA_IM_RAX, BA_IM_IMM, 2);
	ba_AddIM(&ctr->im, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(&ctr->im, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(&ctr->im, 3, BA_IM_SUB, BA_IM_RCX, BA_IM_RAX);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_R8, BA_IM_RDX);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RCX);
	// digits[len++] = lastDigit
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RBP);
	ba_AddIM(&ctr->im, 3, BA_IM_SUB, BA_IM_RAX, BA_IM_R10);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RCX, BA_IM_IMM, 7);
	ba_AddIM(&ctr->im, 3, BA_IM_AND, BA_IM_RCX, BA_IM_R9);
	ba_AddIM(&ctr->im, 4, BA_IM_XOR, BA_IM_RCX, BA_IM_IMM, 7);
	ba_AddIM(&ctr->im, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RCX);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RAX, BA_IM_DL);
	ba_AddIM(&ctr->im, 2, BA_IM_INC, BA_IM_R9);
	ba_AddIM(&ctr->im, 3, BA_IM_TEST, BA_IM_R8, BA_IM_R8);
	ba_AddIM(&ctr->im, 2, BA_IM_LABELJNZ, ctr->labelCnt-3);
	ba_AddIM(&ctr->im, 4, BA_IM_SHL, BA_IM_RCX, BA_IM_IMM, 3);
	// Since r8 is now 0, we can reuse it. It is now used for masking
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_R8, BA_IM_IMM, -1llu);
	ba_AddIM(&ctr->im, 3, BA_IM_SHR, BA_IM_R8, BA_IM_CL);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RDI, BA_IM_R8);
	ba_AddIM(&ctr->im, 2, BA_IM_NOT, BA_IM_RDI);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RBP);
	ba_AddIM(&ctr->im, 3, BA_IM_SUB, BA_IM_RDX, BA_IM_R10);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_ADR, BA_IM_RDX);
	ba_AddIM(&ctr->im, 3, BA_IM_ROR, BA_IM_RAX, BA_IM_CL);
	ba_AddIM(&ctr->im, 5, BA_IM_ADD, BA_IM_ADRSUB, BA_IM_RBP, 0x30, BA_IM_RAX);
	ba_AddIM(&ctr->im, 4, BA_IM_SUB, BA_IM_RDX, BA_IM_IMM, 0x08);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RSI, BA_IM_ADR, BA_IM_RDX);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RSI);
	ba_AddIM(&ctr->im, 3, BA_IM_ROR, BA_IM_RAX, BA_IM_CL);
	ba_AddIM(&ctr->im, 3, BA_IM_AND, BA_IM_RAX, BA_IM_RDI);
	ba_AddIM(&ctr->im, 5, BA_IM_ADD, BA_IM_ADRSUB, BA_IM_RBP, 0x30, BA_IM_RAX);
	ba_AddIM(&ctr->im, 3, BA_IM_ROR, BA_IM_RSI, BA_IM_CL);
	ba_AddIM(&ctr->im, 3, BA_IM_AND, BA_IM_RSI, BA_IM_R8);
	ba_AddIM(&ctr->im, 5, BA_IM_ADD, BA_IM_ADRSUB, BA_IM_RBP, 0x28, BA_IM_RSI);
	ba_AddIM(&ctr->im, 4, BA_IM_SUB, BA_IM_RDX, BA_IM_IMM, 0x08);
	ba_AddIM(&ctr->im, 4, BA_IM_MOV, BA_IM_RSI, BA_IM_ADR, BA_IM_RDX);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RSI);
	ba_AddIM(&ctr->im, 3, BA_IM_ROR, BA_IM_RAX, BA_IM_CL);
	ba_AddIM(&ctr->im, 3, BA_IM_AND, BA_IM_RAX, BA_IM_RDI);
	ba_AddIM(&ctr->im, 5, BA_IM_ADD, BA_IM_ADRSUB, BA_IM_RBP, 0x28, BA_IM_RAX);
	ba_AddIM(&ctr->im, 3, BA_IM_ROR, BA_IM_RSI, BA_IM_CL);
	ba_AddIM(&ctr->im, 3, BA_IM_AND, BA_IM_RSI, BA_IM_R8);
	ba_AddIM(&ctr->im, 5, BA_IM_ADD, BA_IM_ADRSUB, BA_IM_RBP, 0x20, BA_IM_RSI);

	ba_AddIM(&ctr->im, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(&ctr->im, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_R9);
	ba_AddIM(&ctr->im, 2, BA_IM_PUSH, BA_IM_RBX);

	ctr->im->vals = malloc(sizeof(u64));
	if (!ctr->im->vals) {
		ba_ErrorMallocNoMem();
	}
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = ctr->entryIM;
	ctr->im = oldIM;
}

#endif
