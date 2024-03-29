// See LICENSE for copyright/license information

#include "reg.h"
#include "im.h"

// TODO: assign some registers to variables rather than intermediate values

u64 ba_IMToCtrReg(u64 reg) {
	switch (reg) {
		case BA_IM_RAX: return BA_CTRREG_RAX;
		case BA_IM_RCX: return BA_CTRREG_RCX;
		case BA_IM_RDX: return BA_CTRREG_RDX;
		case BA_IM_RBX: return BA_CTRREG_RBX;
		case BA_IM_RSI: return BA_CTRREG_RSI;
		case BA_IM_RDI: return BA_CTRREG_RDI;
		case BA_IM_R8:  return BA_CTRREG_R8;
		case BA_IM_R9:  return BA_CTRREG_R9;
		case BA_IM_R10: return BA_CTRREG_R10;
		case BA_IM_R11: return BA_CTRREG_R11;
		case BA_IM_R12: return BA_CTRREG_R12;
		case BA_IM_R13: return BA_CTRREG_R13;
		case BA_IM_R14: return BA_CTRREG_R14;
		case BA_IM_R15: return BA_CTRREG_R15;
		default: return 0;
	}
}

u64 ba_CtrRegToIM(u64 reg) {
	switch (reg) {
		case BA_CTRREG_RAX: return BA_IM_RAX;
		case BA_CTRREG_RCX: return BA_IM_RCX;
		case BA_CTRREG_RDX: return BA_IM_RDX;
		case BA_CTRREG_RBX: return BA_IM_RBX;
		case BA_CTRREG_RSI: return BA_IM_RSI;
		case BA_CTRREG_RDI: return BA_IM_RDI;
		case BA_CTRREG_R8:  return BA_IM_R8;
		case BA_CTRREG_R9:  return BA_IM_R9;
		case BA_CTRREG_R10: return BA_IM_R10;
		case BA_CTRREG_R11: return BA_IM_R11;
		case BA_CTRREG_R12: return BA_IM_R12;
		case BA_CTRREG_R13: return BA_IM_R13;
		case BA_CTRREG_R14: return BA_IM_R14;
		case BA_CTRREG_R15: return BA_IM_R15;
		default: return 0;
	}
}

u64 ba_NextIMRegister(struct ba_Ctr* ctr) {
	u64 ctrReg = BA_CTRREG_RAX;
	while (ctrReg <= BA_CTRREG_R15) {
		if (!(ctr->usedRegisters & ctrReg)) {
			ctr->usedRegisters |= ctrReg;
			return ba_CtrRegToIM(ctrReg);
		}
		ctrReg <<= 1;
	}

	// Stack
	return 0;
}

// Note: does not modify ctr->funcFrameStk, ctr->usedRegisters
void ba_UsedRegPreserve(struct ba_Ctr* ctr) {
	u64 ctrReg = BA_CTRREG_RAX;
	while (ctrReg <= BA_CTRREG_R15) {
		if (ctr->usedRegisters & ctrReg) {
			ba_AddIM(ctr, 2, BA_IM_PUSH, ba_CtrRegToIM(ctrReg));
			ctr->imStackSize += 8;
		}
		ctrReg <<= 1;
	}
}

// Note: does not modify ctr->funcFrameStk, ctr->usedRegisters
void ba_UsedRegRestore(struct ba_Ctr* ctr, u64 until) {
	u64 ctrReg = BA_CTRREG_R15;
	while (ctrReg >= (BA_CTRREG_RAX << until)) {
		if (ctr->usedRegisters & ctrReg) {
			ba_AddIM(ctr, 2, BA_IM_POP, ba_CtrRegToIM(ctrReg));
			ctr->imStackSize -= 8;
		}
		ctrReg >>= 1;
	}
}

