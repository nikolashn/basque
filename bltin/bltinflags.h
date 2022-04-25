// See LICENSE for copyright/license information

#ifndef BA__BLTINFLAGS_H
#define BA__BLTINFLAGS_H

enum {
	BA_BLTIN_U64ToStr = 0,
	BA_BLTIN_MemCpy = 1,
	
	BA_BLTIN__COUNT = 2
};

u64 ba_BltinFlags[1] = {0};
u64 ba_BltinLabels[BA_BLTIN__COUNT] = {0};

void ba_BltinFlagsSet(u64 flag) {
	ba_BltinFlags[flag >> 6] |= 1 << (flag & 63);
}

u64 ba_BltinFlagsTest(u64 flag) {
	return ba_BltinFlags[flag >> 6] & (1 << (flag & 63));
}

#endif
