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
	BA_IM_ADRMADD      = 0x5,
	BA_IM_ADRMSUB      = 0x6,
	BA_IM_DATASGMT     = 0x7,
	
	// Normal assembly instructions
	BA_IM_MOV          = 0x10,
	BA_IM_ADD          = 0x11,
	BA_IM_SUB          = 0x12,
	BA_IM_SYSCALL      = 0x13,
	
	// Registers must remain in order, otherwise binary generation messes up
	// i.e. the last nibble of each value must stay the same
	BA_IM_RAX          = 0xc0,
	BA_IM_RCX          = 0xc1,
	BA_IM_RDX          = 0xc2,
	BA_IM_RBX          = 0xc3,
	BA_IM_RSP          = 0xc4,
	BA_IM_RBP          = 0xc5,
	BA_IM_RSI          = 0xc6,
	BA_IM_RDI          = 0xc7,
	BA_IM_R8           = 0xc8,
	BA_IM_R9           = 0xc9,
	BA_IM_R10          = 0xca,
	BA_IM_R11          = 0xcb,
	BA_IM_R12          = 0xcc,
	BA_IM_R13          = 0xcd,
	BA_IM_R14          = 0xce,
	BA_IM_R15          = 0xcf,
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
	u8 adrP2 = 0;

	for (u64 i = 0; i < im->count; i++) {
		u64 val = im->vals[i];
	
		if (isImm) {
			sprintf(str+strlen(str), "%#llx ", val);
			isImm = 0;
			if (adrP2 == 1) {
				isImm = adrP2--;
			}
			continue;
		}
		else if (adrP1) {
			if (!--adrP1) {
				isImm = 1;
			}
		}
		else if (adrP2) {
			if (--adrP1 <= 1) {
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
			case BA_IM_ADRMADD:
				strcat(str, "ADRMADD ");
				adrP2 = 2;
				break;
			case BA_IM_ADRMSUB:
				strcat(str, "ADRMSUB ");
				adrP2 = 2;
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
