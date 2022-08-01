// See LICENSE for copyright/license information

#include "im.h"

struct ba_IM* ba_NewIM() {
	struct ba_IM* im = ba_CAlloc(1, sizeof(*im));
	return im;
}

struct ba_IM* ba_DelIM(struct ba_IM* im) {
	struct ba_IM* next = im->next;
	free(im->vals);
	free(im);
	return next;
}

u64 ba_AdjRegSize(u64 reg, u64 size) {
	if (size == 8) {
		return reg;
	}
	if (size == 1) {
		return reg - BA_IM_RAX + BA_IM_AL;
	}
	fprintf(stderr, "Error: invalid register size %llu\n", size);
	exit(-1);
}

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
	char* str = ba_MAlloc(255);
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

