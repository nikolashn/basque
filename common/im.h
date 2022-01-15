// See LICENSE for copyright/license information

#ifndef BA__IM_H
#define BA__IM_H

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

struct ba_IM* ba_NewIM() {
	struct ba_IM* im = calloc(1, sizeof(*im));
	if (!im) {
		ba_ErrorMallocNoMem();
	}
	return im;
}

struct ba_IM* ba_DelIM(struct ba_IM* im) {
	struct ba_IM* next = im->next;
	free(im->vals);
	free(im);
	return next;
}

struct ba_IMLabel {
	u64 addr;
	struct ba_DynArr64* jmpOfsts;
	struct ba_DynArr8* jmpOfstSizes;
	bool isFound;
};

char* ba_IMItemToStr(u64 val) {
	switch (val) {
		case BA_IM_NOP:          return "NOP ";
		case BA_IM_IMM:          return "IMM ";
		case BA_IM_LABEL:        return "LABEL ";
		case BA_IM_ADR:          return "ADR ";
		case BA_IM_ADRADD:       return "ADRADD ";
		case BA_IM_ADRSUB:       return "ADRSUB ";
		case BA_IM_ADRADDREGMUL: return "ADRADDREGMUL ";
		case BA_IM_MOV:          return "MOV ";
		case BA_IM_ADD:          return "ADD ";
		case BA_IM_SUB:          return "SUB ";
		case BA_IM_CMP:          return "CMP ";
		case BA_IM_SYSCALL:      return "SYSCALL ";
		case BA_IM_LABELCALL:    return "LABELCALL ";
		case BA_IM_RET:          return "RET ";
		case BA_IM_PUSH:         return "PUSH ";
		case BA_IM_POP:          return "POP ";
		case BA_IM_LEA:          return "LEA ";
		case BA_IM_GOTO:         return "GOTO ";
		case BA_IM_LABELJMP:     return "LABELJMP ";
		case BA_IM_LABELJZ:      return "LABELJZ ";
		case BA_IM_LABELJNZ:     return "LABELJNZ ";
		case BA_IM_LABELJB:      return "LABELJB ";
		case BA_IM_LABELJBE:     return "LABELJBE ";
		case BA_IM_LABELJA:      return "LABELJA ";
		case BA_IM_LABELJAE:     return "LABELJAE ";
		case BA_IM_LABELJL:      return "LABELJL ";
		case BA_IM_LABELJLE:     return "LABELJLE ";
		case BA_IM_LABELJG:      return "LABELJG ";
		case BA_IM_LABELJGE:     return "LABELJGE ";
		case BA_IM_MOVZX:        return "MOVZX ";
		case BA_IM_CQO:          return "CQO ";
		case BA_IM_SETS:         return "SETS ";
		case BA_IM_SETNS:        return "SETNS ";
		case BA_IM_SETZ:         return "SETZ ";
		case BA_IM_SETNZ:        return "SETNZ ";
		case BA_IM_SETB:         return "SETB ";
		case BA_IM_SETBE:        return "SETBE ";
		case BA_IM_SETA:         return "SETA ";
		case BA_IM_SETAE:        return "SETAE ";
		case BA_IM_SETL:         return "SETL ";
		case BA_IM_SETLE:        return "SETLE ";
		case BA_IM_SETG:         return "SETG ";
		case BA_IM_SETGE:        return "SETGE ";
		case BA_IM_INC:          return "INC ";
		case BA_IM_DEC:          return "DEC ";
		case BA_IM_NOT:          return "NOT ";
		case BA_IM_NEG:          return "NEG ";
		case BA_IM_TEST:         return "TEST ";
		case BA_IM_AND:          return "AND ";
		case BA_IM_XOR:          return "XOR ";
		case BA_IM_OR:           return "OR ";
		case BA_IM_ROL:          return "ROL ";
		case BA_IM_ROR:          return "ROR ";
		case BA_IM_SHL:          return "SHL ";
		case BA_IM_SHR:          return "SHR ";
		case BA_IM_SAR:          return "SAR ";
		case BA_IM_MUL:          return "MUL ";
		case BA_IM_IMUL:         return "IMUL ";
		case BA_IM_DIV:          return "DIV ";
		case BA_IM_IDIV:         return "IDIV ";
		case BA_IM_RAX:          return "RAX ";
		case BA_IM_RBX:          return "RBX ";
		case BA_IM_RCX:          return "RCX ";
		case BA_IM_RDX:          return "RDX ";
		case BA_IM_RSI:          return "RSI ";
		case BA_IM_RDI:          return "RDI ";
		case BA_IM_RSP:          return "RSP ";
		case BA_IM_RBP:          return "RBP ";
		case BA_IM_R8:           return "R8 ";
		case BA_IM_R9:           return "R9 ";
		case BA_IM_R10:          return "R10 ";
		case BA_IM_R11:          return "R11 ";
		case BA_IM_R12:          return "R12 ";
		case BA_IM_R13:          return "R13 ";
		case BA_IM_R14:          return "R14 ";
		case BA_IM_R15:          return "R15 ";
		case BA_IM_AL:           return "AL ";
		case BA_IM_BL:           return "BL ";
		case BA_IM_CL:           return "CL ";
		case BA_IM_DL:           return "DL ";
		case BA_IM_SIL:          return "SIL ";
		case BA_IM_DIL:          return "DIL ";
		case BA_IM_SPL:          return "SPL ";
		case BA_IM_BPL:          return "BPL ";
		case BA_IM_R8B:          return "R8B ";
		case BA_IM_R9B:          return "R9B ";
		case BA_IM_R10B:         return "R10B ";
		case BA_IM_R11B:         return "R11B ";
		case BA_IM_R12B:         return "R12B ";
		case BA_IM_R13B:         return "R13B ";
		case BA_IM_R14B:         return "R14B ";
		case BA_IM_R15B:         return "R15B ";
	}
	return 0;
}

char* ba_IMToStr(struct ba_IM* im) {
	char* str = malloc(255);
	if (!str) {
		ba_ErrorMallocNoMem();
	}
	*str = 0;

	bool isImm = 0;
	u8 adrP1 = 0;

	for (u64 i = 0; i < im->count; i++) {
		u64 val = im->vals[i];
	
		if (isImm) {
			sprintf(str+strlen(str), "%#llx ", val);
			isImm = 0;
			goto BA_LBL_IMTOSTR_LOOPEND;
		}
		else if (adrP1 && !--adrP1) {
			isImm = 1;
		}
		
		char* valStr = ba_IMItemToStr(val);
		if (valStr) {
			strcat(str, valStr);
		}
		else {
			sprintf(str+strlen(str), "%llu ", val);
		}
		switch (val) {
			case BA_IM_IMM: case BA_IM_LABEL: case BA_IM_ADRADDREGMUL:
			case BA_IM_LABELCALL: case BA_IM_RET: case BA_IM_GOTO:
			case BA_IM_LABELJMP: case BA_IM_LABELJZ: case BA_IM_LABELJNZ:
			case BA_IM_LABELJB: case BA_IM_LABELJBE: case BA_IM_LABELJA:
			case BA_IM_LABELJAE: case BA_IM_LABELJL: case BA_IM_LABELJLE:
			case BA_IM_LABELJG: case BA_IM_LABELJGE: case BA_IM_MOVZX:
				isImm = 1;
				break;
			case BA_IM_ADRADD:
			case BA_IM_ADRSUB:
				adrP1 = 1;
				break;
		}
		BA_LBL_IMTOSTR_LOOPEND:;
	}

	return str;
}

#endif
