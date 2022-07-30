// See LICENSE for copyright/license information

#ifndef BA__COMMON_REG_H
#define BA__COMMON_REG_H

#include "ctr.h"

/* Be careful when changing these values, they are bit shifts in this order 
 * because of how ba_NextIMRegister is calculated */
enum {
	BA_CTRREG_RAX = 1 << 0,
	BA_CTRREG_RCX = 1 << 1,
	BA_CTRREG_RDX = 1 << 2,
	BA_CTRREG_RSI = 1 << 3,
	BA_CTRREG_RDI = 1 << 4,
	BA_CTRREG_R8  = 1 << 5,
	BA_CTRREG_R9  = 1 << 6,
	BA_CTRREG_R10 = 1 << 7,
	BA_CTRREG_R11 = 1 << 8,
	BA_CTRREG_R12 = 1 << 9,
	BA_CTRREG_R13 = 1 << 10,
	BA_CTRREG_R14 = 1 << 11,
	BA_CTRREG_R15 = 1 << 12
};

u64 ba_IMToCtrReg(u64 reg);
u64 ba_CtrRegToIM(u64 reg);
u64 ba_NextIMRegister(struct ba_Ctr* ctr);
void ba_UsedRegPreserve(struct ba_Ctr* ctr);
void ba_UsedRegRestore(struct ba_Ctr* ctr, u64 until);

#endif
