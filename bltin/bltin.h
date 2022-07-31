// See LICENSE for copyright/license information

#ifndef BA__BLTIN_H
#define BA__BLTIN_H

#include "u64tostr.h"
#include "memcopy.h"
#include "sys.h"

enum {
	BA_BLTIN_U64ToStr   = 0,
	BA_BLTIN_MemCopy    = 1,
	
	BA_BLTIN_Sys        = 2,
	BA_BLTIN_SysRead    = 3,
	BA_BLTIN_SysWrite   = 4,
	BA_BLTIN_SysOpen    = 5,
	BA_BLTIN_SysClose   = 6,
	BA_BLTIN_SysStat    = 7,
	BA_BLTIN_SysFStat   = 8,
	BA_BLTIN_SysLStat   = 9,
	BA_BLTIN_SysPoll    = 10,
	BA_BLTIN_SysLSeek   = 11,
	
	BA_BLTIN__COUNT     = 12,
	BA_BLTIN_FLAG_CNT   = 2, // ceil(BA_BLTIN__COUNT / 8.0)
};

void ba_BltinFlagsSet(u64 flag);
u64 ba_BltinFlagsTest(u64 flag);
void ba_BltinLblSet(u64 bltin, u64 lbl);
u64 ba_BltinLblGet(u64 bltin);

#endif
