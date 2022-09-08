// See LICENSE for copyright/license information

#include "bltin.h"

/* Params (stack): int to convert
 * Returns (rax): string length; (stack): string (0x18) */
void ba_BltinU64ToStr(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_FormatU64ToStr);
	ba_BltinLblSet(BA_BLTIN_FormatU64ToStr, ctr->labelCnt);
	ctr->labelCnt += 7;
	
	struct ba_IM* oldIM = ctr->im;
	struct ba_IM* oldStartIM = ctr->startIM;

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-7); // U64ToStr
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x18, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x19, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, 0x18);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RSP);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5); // MakeDigitsStart

	// lastDigit = num % 10; num /= 10
	// uses magic to divide without using div instruction
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, 0xcccccccccccccccd);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RCX);
	ba_AddIM(ctr, 2, BA_IM_MUL, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_SHR, BA_IM_RDX, BA_IM_IMM, 3);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_SHL, BA_IM_RAX, BA_IM_IMM, 2);
	ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RCX, BA_IM_RAX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RDI, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RCX);

	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_DL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBX, 0x30);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDI, BA_IM_ADRSUB, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 3, BA_IM_CMP, BA_IM_RBX, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_LABELJL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_ADR, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RDX, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RDX, BA_IM_RSI);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);

	ctr->im->vals = ba_MAlloc(sizeof(u64));
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = oldStartIM;
	ctr->im = oldIM;
}

/* Params (stack): int to convert
 * Returns (rax): string length; (stack): string (0x18) */
void ba_BltinI64ToStr(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_FormatI64ToStr);
	ba_BltinLblSet(BA_BLTIN_FormatI64ToStr, ctr->labelCnt);
	ctr->labelCnt += 9;
	
	struct ba_IM* oldIM = ctr->im;
	struct ba_IM* oldStartIM = ctr->startIM;

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-9);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RSI, BA_IM_RSI);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_LABELJG, ctr->labelCnt-7);
	ba_AddIM(ctr, 2, BA_IM_LABELJL, ctr->labelCnt-8);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x18, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x19, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 0x1);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-8);
	ba_AddIM(ctr, 2, BA_IM_NEG, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_SIL, BA_IM_IMM, 1);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-7);
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, 0x18);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RSP);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-5);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-6);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RCX, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RDX, BA_IM_IMM, 0xcccccccccccccccd);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RCX);
	ba_AddIM(ctr, 2, BA_IM_MUL, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_SHR, BA_IM_RDX, BA_IM_IMM, 3);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_SHL, BA_IM_RAX, BA_IM_IMM, 2);
	ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RCX, BA_IM_RAX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RDI, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RDX, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_DL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDI, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-6);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RSI, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_LABELJZ, ctr->labelCnt-4);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_DL, BA_IM_IMM, 0xfd);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_DL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBX, 0x30);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDI, BA_IM_ADRSUB, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3);
	ba_AddIM(ctr, 3, BA_IM_CMP, BA_IM_RBX, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_LABELJL, ctr->labelCnt-2);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_ADR, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 3, BA_IM_LABELJMP, ctr->labelCnt-3);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RDX, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RDX, BA_IM_RSI);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);

	ctr->im->vals = ba_MAlloc(sizeof(u64));
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = oldStartIM;
	ctr->im = oldIM;
}

/* Params (stack): int to convert
 * Returns (rax): string length; (stack): string (0x18) */
void ba_BltinHexToStr(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_FormatHexToStr);
	ba_BltinLblSet(BA_BLTIN_FormatHexToStr, ctr->labelCnt);
	ctr->labelCnt += 7;
	
	struct ba_IM* oldIM = ctr->im;
	struct ba_IM* oldStartIM = ctr->startIM;

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-7); // HexToStr
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 0x6665646362613938);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 0x3736353433323130);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RAX);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x18, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x19, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, 0x18);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RSP);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_AND, BA_IM_AL, BA_IM_IMM, 0xf);
	ba_AddIM(ctr, 4, BA_IM_SHR, BA_IM_RDX, BA_IM_IMM, 4);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBX, 0x40);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDI, BA_IM_ADRSUB, BA_IM_RBP, 0x28);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 3, BA_IM_CMP, BA_IM_RBX, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_LABELJL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_ADR, BA_IM_RDI);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RCX, BA_IM_ADRSUB, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 3, BA_IM_ADD, BA_IM_RCX, BA_IM_RAX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_ADR, BA_IM_RCX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RDX, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RDX, BA_IM_RSI);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);

	ctr->im->vals = ba_MAlloc(sizeof(u64));
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = oldStartIM;
	ctr->im = oldIM;
}

/* Params (stack): int to convert
 * Returns (rax): string length; (stack): string (0x18) */
void ba_BltinOctToStr(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_FormatOctToStr);
	ba_BltinLblSet(BA_BLTIN_FormatOctToStr, ctr->labelCnt);
	ctr->labelCnt += 7;
	
	struct ba_IM* oldIM = ctr->im;
	struct ba_IM* oldStartIM = ctr->startIM;

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-7); // OctToStr
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x18, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x19, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, 0x18);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RSP);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_AND, BA_IM_AL, BA_IM_IMM, 7);
	ba_AddIM(ctr, 4, BA_IM_SHR, BA_IM_RDX, BA_IM_IMM, 3);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBX, 0x30);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDI, BA_IM_ADRSUB, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 3, BA_IM_CMP, BA_IM_RBX, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_LABELJL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_ADR, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RDX, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RDX, BA_IM_RSI);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);

	ctr->im->vals = ba_MAlloc(sizeof(u64));
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = oldStartIM;
	ctr->im = oldIM;
}

/* Params (stack): int to convert
 * Returns (rax): string length; (stack): string (0x40) */
void ba_BltinBinToStr(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_FormatBinToStr);
	ba_BltinLblSet(BA_BLTIN_FormatBinToStr, ctr->labelCnt);
	ctr->labelCnt += 7;
	
	struct ba_IM* oldIM = ctr->im;
	struct ba_IM* oldStartIM = ctr->startIM;

	ctr->startIM = ba_NewIM();
	ctr->im = ctr->startIM;

	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-7); // BinToStr
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x18, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_ADRADD, BA_IM_RBP, 0x19, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-6); // MakeDigits
	ba_AddIM(ctr, 4, BA_IM_SUB, BA_IM_RSP, BA_IM_IMM, 0x40);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RSP);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 4, BA_IM_AND, BA_IM_AL, BA_IM_IMM, 1);
	ba_AddIM(ctr, 4, BA_IM_SHR, BA_IM_RDX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-4); // MakeDigitsTest
	ba_AddIM(ctr, 3, BA_IM_TEST, BA_IM_RDX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABELJNZ, ctr->labelCnt-5); // MakeDigitsStart
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBX, 0x58);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_LEA, BA_IM_RDI, BA_IM_ADRSUB, BA_IM_RBP, 0x40);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 3, BA_IM_CMP, BA_IM_RBX, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_LABELJL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_AL, BA_IM_ADR, BA_IM_RDI);
	ba_AddIM(ctr, 4, BA_IM_ADD, BA_IM_AL, BA_IM_IMM, 0x30);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RBX, BA_IM_AL);
	ba_AddIM(ctr, 2, BA_IM_INC, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_DEC, BA_IM_RBX);
	ba_AddIM(ctr, 2, BA_IM_LABELJMP, ctr->labelCnt-3); // ReverseLoopStart
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-2); // ReverseLoopEnd
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_AL, BA_IM_AL);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_ADR, BA_IM_RDX, BA_IM_AL);
	ba_AddIM(ctr, 3, BA_IM_SUB, BA_IM_RDX, BA_IM_RSI);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RAX, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1); // Epilogue
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RSP, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP);

	ctr->im->vals = ba_MAlloc(sizeof(u64));
	ctr->im->vals[0] = BA_IM_RET;
	ctr->im->count = 1;
	ctr->im->next = oldStartIM;
	ctr->im = oldIM;
}

