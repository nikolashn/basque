// See LICENSE for copyright/license information

#include "bltin.h"

u64 bltinLbls[BA_BLTIN__COUNT] = {0};
u64 bltinFlags[BA_BLTIN_FLAG_CNT] = {0};

void ba_BltinFlagsSet(u64 flag) {
	bltinFlags[flag >> 6] |= 1 << (flag & 63);
}

u64 ba_BltinFlagsTest(u64 flag) {
	return bltinFlags[flag >> 6] & (1 << (flag & 63));
}

void ba_BltinLblSet(u64 bltin, u64 lbl) {
	bltinLbls[bltin] = lbl;
}

u64 ba_BltinLblGet(u64 bltin) {
	return bltinLbls[bltin];
}

