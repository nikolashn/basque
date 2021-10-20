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
	BA_IM_LABEL        = 0x5,
	BA_IM_DATASGMT     = 0xf,
	
	// Normal assembly instructions
	BA_IM_MOV          = 0x10,
	BA_IM_ADD          = 0x11,
	BA_IM_SUB          = 0x12,
	BA_IM_INC          = 0x13,
	BA_IM_NOT          = 0x14,
	BA_IM_TEST         = 0x15,
	BA_IM_AND          = 0x16,
	BA_IM_XOR          = 0x17,
	BA_IM_SHL          = 0x18,
	BA_IM_SHR          = 0x19,
	BA_IM_ROL          = 0x1a,
	BA_IM_ROR          = 0x1b,
	BA_IM_MUL          = 0x1c,
	
	BA_IM_SYSCALL      = 0x40,
	BA_IM_LABELCALL    = 0x41,
	BA_IM_RET          = 0x42,
	
	BA_IM_LABELJMP     = 0x50,
	BA_IM_LABELJNZ     = 0x51,

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
};

char* ba_IMToStr(struct ba_IM* im) {
	char* str = malloc(255);
	if (!str) {
		ba_ErrorMallocNoMem();
	}
	*str = 0;

	u8 isImm = 0;
	u8 adrP1 = 0;

	for (u64 i = 0; i < im->count; i++) {
		u64 val = im->vals[i];
	
		if (isImm) {
			sprintf(str+strlen(str), "%#llx ", val);
			isImm = 0;
			continue;
		}
		else if (adrP1) {
			if (!--adrP1) {
				isImm = 1;
			}
		}

		switch (val) {
			case BA_IM_NOP:
				strcat(str, "NOP ");
				break;
			case BA_IM_IMM:
				strcat(str, "IMM ");
				isImm = 1;
				break;
			case BA_IM_DATASGMT:
				strcat(str, "DATASGMT ");
				isImm = 1;
				break;
			case BA_IM_LABEL:
				strcat(str, "LABEL ");
				isImm = 1;
				break;
			case BA_IM_ADR:
				strcat(str, "ADR ");
				break;
			case BA_IM_ADRADD:
				strcat(str, "ADRADD ");
				adrP1 = 1;
				break;
			case BA_IM_ADRSUB:
				strcat(str, "ADRSUB ");
				adrP1 = 1;
				break;
			case BA_IM_MOV:
				strcat(str, "MOV ");
				break;
			case BA_IM_ADD:
				strcat(str, "ADD ");
				break;
			case BA_IM_SUB:
				strcat(str, "SUB ");
				break;
			case BA_IM_SYSCALL:
				strcat(str, "SYSCALL ");
				break;
			case BA_IM_LABELCALL:
				strcat(str, "LABELCALL ");
				isImm = 1;
				break;
			case BA_IM_RET:
				strcat(str, "RET ");
				isImm = 1;
				break;
			case BA_IM_LABELJMP:
				strcat(str, "LABELJMP ");
				isImm = 1;
				break;
			case BA_IM_LABELJNZ:
				strcat(str, "LABELJNZ ");
				isImm = 1;
				break;
			case BA_IM_INC:
				strcat(str, "INC ");
				break;
			case BA_IM_NOT:
				strcat(str, "NOT ");
				break;
			case BA_IM_TEST:
				strcat(str, "TEST ");
				break;
			case BA_IM_AND:
				strcat(str, "AND ");
				break;
			case BA_IM_XOR:
				strcat(str, "XOR ");
				break;
			case BA_IM_SHL:
				strcat(str, "SHL ");
				break;
			case BA_IM_SHR:
				strcat(str, "SHR ");
				break;
			case BA_IM_ROL:
				strcat(str, "ROL ");
				break;
			case BA_IM_ROR:
				strcat(str, "ROR ");
				break;
			case BA_IM_RAX:
				strcat(str, "RAX ");
				break;
			case BA_IM_RBX:
				strcat(str, "RBX ");
				break;
			case BA_IM_RCX:
				strcat(str, "RCX ");
				break;
			case BA_IM_RDX:
				strcat(str, "RDX ");
				break;
			case BA_IM_RSI:
				strcat(str, "RSI ");
				break;
			case BA_IM_RDI:
				strcat(str, "RDI ");
				break;
			case BA_IM_RSP:
				strcat(str, "RSP ");
				break;
			case BA_IM_RBP:
				strcat(str, "RBP ");
				break;
			case BA_IM_R8:
				strcat(str, "R8 ");
				break;
			case BA_IM_R9:
				strcat(str, "R9 ");
				break;
			case BA_IM_R10:
				strcat(str, "R10 ");
				break;
			case BA_IM_R11:
				strcat(str, "R11 ");
				break;
			case BA_IM_R12:
				strcat(str, "R12 ");
				break;
			case BA_IM_R13:
				strcat(str, "R13 ");
				break;
			case BA_IM_R14:
				strcat(str, "R14 ");
				break;
			case BA_IM_R15:
				strcat(str, "R15 ");
				break;
			default:
				sprintf(str+strlen(str), "%llu ", val);
		}
	}

	return str;
}

#endif
