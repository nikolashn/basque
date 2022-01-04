// See LICENSE for copyright/license information

#ifndef BA__REG_H
#define BA__REG_H

enum {
	BA_CTRREG_RAX = 0x1,
	BA_CTRREG_RCX = 0x2,
	BA_CTRREG_RDX = 0x4,
	BA_CTRREG_RBX = 0x8,
	BA_CTRREG_RSI = 0x10,
	BA_CTRREG_RDI = 0x20,
};

u64 ba_IMToCtrRegister(u64 imReg) {
	switch (imReg) {
		case BA_IM_RAX:
			return BA_CTRREG_RAX;
		case BA_IM_RCX:
			return BA_CTRREG_RCX;
		case BA_IM_RDX:
			return BA_CTRREG_RDX;
		case BA_IM_RBX:
			return BA_CTRREG_RBX;
		case BA_IM_RSI:
			return BA_CTRREG_RSI;
		case BA_IM_RDI:
			return BA_CTRREG_RDI;
		default:
			return 0;
	}
}

u64 ba_NextIMRegister(struct ba_Controller* ctr) {
	if (!(ctr->usedRegisters & BA_CTRREG_RAX)) {
		ctr->usedRegisters |= BA_CTRREG_RAX;
		return BA_IM_RAX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RCX)) {
		ctr->usedRegisters |= BA_CTRREG_RCX;
		return BA_IM_RCX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RDX)) {
		ctr->usedRegisters |= BA_CTRREG_RDX;
		return BA_IM_RDX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RBX)) {
		ctr->usedRegisters |= BA_CTRREG_RBX;
		return BA_IM_RBX;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RSI)) {
		ctr->usedRegisters |= BA_CTRREG_RSI;
		return BA_IM_RSI;
	}
	if (!(ctr->usedRegisters & BA_CTRREG_RDI)) {
		ctr->usedRegisters |= BA_CTRREG_RDI;
		return BA_IM_RDI;
	}

	// Stack
	return 0;
}

u64 ba_GetIMRegisterFlag(u64 reg) {
	switch (reg) {
		case BA_IM_RAX:
			return BA_CTRREG_RAX;
		case BA_IM_RCX:
			return BA_CTRREG_RCX;
		case BA_IM_RDX:
			return BA_CTRREG_RDX;
		case BA_IM_RBX:
			return BA_CTRREG_RBX;
		case BA_IM_RSI:
			return BA_CTRREG_RSI;
		case BA_IM_RDI:
			return BA_CTRREG_RDI;
		default:
			return 0;
	}
}

#endif
