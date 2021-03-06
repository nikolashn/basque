// See LICENSE for copyright/license information

#ifndef BA__COMMON_IM_H
#define BA__COMMON_IM_H

#include "exitmsg.h"

enum {
	BA_IM_NOP          = 0x0,
	
	// My special syntax
	BA_IM_IMM          = 0x1,
	BA_IM_ADR          = 0x2,
	BA_IM_ADRADD       = 0x3,
	BA_IM_ADRSUB       = 0x4,
	BA_IM_ADRADDREGMUL = 0x5,
	BA_IM_64ADR        = 0x6,
	BA_IM_64ADRADD     = 0x7,
	BA_IM_64ADRSUB     = 0x8,
	BA_IM_LABEL        = 0x9,
	BA_IM_STATIC       = 0xa,
	
	// Normal assembly instructions
	BA_IM_MOV          = 0x10,
	BA_IM_ADD          = 0x11,
	BA_IM_SUB          = 0x12,
	BA_IM_CMP          = 0x13,
	BA_IM_INC          = 0x14,
	BA_IM_DEC          = 0x15,
	BA_IM_NOT          = 0x16,
	BA_IM_NEG          = 0x17,
	BA_IM_TEST         = 0x18,
	BA_IM_AND          = 0x19,
	BA_IM_XOR          = 0x1a,
	BA_IM_OR           = 0x1b,
	BA_IM_ROL          = 0x1c,
	BA_IM_ROR          = 0x1d,
	BA_IM_SHL          = 0x1e,
	BA_IM_SHR          = 0x1f,
	BA_IM_SAR          = 0x20,
	BA_IM_MUL          = 0x21,
	BA_IM_IMUL         = 0x22,
	BA_IM_DIV          = 0x23,
	BA_IM_IDIV         = 0x24,
	
	BA_IM_SYSCALL      = 0x40,
	BA_IM_LABELCALL    = 0x41,
	BA_IM_RET          = 0x42,
	BA_IM_PUSH         = 0x43,
	BA_IM_POP          = 0x44,
	BA_IM_LEA          = 0x45,
	
	BA_IM_GOTO         = 0x50,
	BA_IM_LABELJMP     = 0x51,
	BA_IM_LABELJZ      = 0x52,
	BA_IM_LABELJNZ     = 0x53,
	BA_IM_LABELJB      = 0x54,
	BA_IM_LABELJBE     = 0x55,
	BA_IM_LABELJA      = 0x56,
	BA_IM_LABELJAE     = 0x57,
	BA_IM_LABELJL      = 0x58,
	BA_IM_LABELJLE     = 0x59,
	BA_IM_LABELJG      = 0x5a,
	BA_IM_LABELJGE     = 0x5b,

	BA_IM_MOVZX        = 0x60,
	BA_IM_CQO          = 0x61,
	BA_IM_SETS         = 0x62,
	BA_IM_SETNS        = 0x63,
	BA_IM_SETZ         = 0x64,
	BA_IM_SETNZ        = 0x65,
	BA_IM_SETB         = 0x66,
	BA_IM_SETBE        = 0x67,
	BA_IM_SETA         = 0x68,
	BA_IM_SETAE        = 0x69,
	BA_IM_SETL         = 0x6a,
	BA_IM_SETLE        = 0x6b,
	BA_IM_SETG         = 0x6c,
	BA_IM_SETGE        = 0x6d,

	// Registers must remain in order, otherwise binary generation messes up
	// i.e. the last nibble of each value must stay the same as originally, 
	// while the bits before those must be the same for each register
	
	// General-purpose registers (GPR)
	BA_IM_RAX          = 0x100,
	BA_IM_RCX          = 0x101,
	BA_IM_RDX          = 0x102,
	BA_IM_RBX          = 0x103,
	BA_IM_RSP          = 0x104,
	BA_IM_RBP          = 0x105,
	BA_IM_RSI          = 0x106,
	BA_IM_RDI          = 0x107,
	BA_IM_R8           = 0x108,
	BA_IM_R9           = 0x109,
	BA_IM_R10          = 0x10a,
	BA_IM_R11          = 0x10b,
	BA_IM_R12          = 0x10c,
	BA_IM_R13          = 0x10d,
	BA_IM_R14          = 0x10e,
	BA_IM_R15          = 0x10f,
	
	// Lowest byte of GPR (GPRb)
	BA_IM_AL           = 0x110,
	BA_IM_CL           = 0x111,
	BA_IM_DL           = 0x112,
	BA_IM_BL           = 0x113,
	BA_IM_SPL          = 0x114,
	BA_IM_BPL          = 0x115,
	BA_IM_SIL          = 0x116,
	BA_IM_DIL          = 0x117,
	BA_IM_R8B          = 0x118,
	BA_IM_R9B          = 0x119,
	BA_IM_R10B         = 0x11a,
	BA_IM_R11B         = 0x11b,
	BA_IM_R12B         = 0x11c,
	BA_IM_R13B         = 0x11d,
	BA_IM_R14B         = 0x11e,
	BA_IM_R15B         = 0x11f,
};

struct ba_IM {
	u64* vals;
	u64 count;
	struct ba_IM* next;
};

struct ba_IMLabel {
	u64 addr;
	struct ba_DynArr64* jmpOfsts;
	struct ba_DynArr8* jmpOfstSizes;
	bool isFound;
};

struct ba_IM* ba_NewIM();
struct ba_IM* ba_DelIM(struct ba_IM* im);
u64 ba_AdjRegSize(u64 reg, u64 size);
char* ba_IMItemToStr(u64 val);
char* ba_IMToStr(struct ba_IM* im);

#endif
