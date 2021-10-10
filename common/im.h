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
	BA_IM_DATASGMT     = 0xf,
	
	// Normal assembly instructions
	BA_IM_MOV          = 0x10,
	BA_IM_ADD          = 0x11,
	BA_IM_SUB          = 0x12,
	BA_IM_INC          = 0x13,
	BA_IM_NOT          = 0x14,
	
	BA_IM_SYSCALL      = 0x40,

	// GPRs must remain in order, otherwise binary generation messes up
	// i.e. the last nibble of each value must stay the same as originally, 
	// while the bits before those must be the same for each register
	BA_IM_RAX          = 0x1c0,
	BA_IM_RCX          = 0x1c1,
	BA_IM_RDX          = 0x1c2,
	BA_IM_RBX          = 0x1c3,
	BA_IM_RSP          = 0x1c4,
	BA_IM_RBP          = 0x1c5,
	BA_IM_RSI          = 0x1c6,
	BA_IM_RDI          = 0x1c7,
	BA_IM_R8           = 0x1c8,
	BA_IM_R9           = 0x1c9,
	BA_IM_R10          = 0x1ca,
	BA_IM_R11          = 0x1cb,
	BA_IM_R12          = 0x1cc,
	BA_IM_R13          = 0x1cd,
	BA_IM_R14          = 0x1ce,
	BA_IM_R15          = 0x1cf,
};

struct ba_IM {
	u64* vals;
	u64 count;
	struct ba_IM* next;
};

struct ba_IM* ba_NewIM() {
	struct ba_IM* im = malloc(sizeof(struct ba_IM));
	if (!im) {
		ba_ErrorMallocNoMem();
	}
	im->vals = 0;
	im->count = 0;
	im->next = 0;
	return im;
}

struct ba_IM* ba_DelIM(struct ba_IM* im) {
	struct ba_IM* next = im->next;
	free(im->vals);
	free(im);
	return next;
}

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
			case BA_IM_INC:
				strcat(str, "INC ");
				break;
			case BA_IM_NOT:
				strcat(str, "NOT ");
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
