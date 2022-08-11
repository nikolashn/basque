// See LICENSE for copyright/license information

#ifndef BA__BLTIN_H
#define BA__BLTIN_H

#include "../common/func.h"
#include "u64tostr.h"
#include "core.h"
#include "sys.h"

enum {
	BA_BLTIN_U64ToStr    = 0,

	BA_BLTIN_CoreMemCopy = 1,
	BA_BLTIN_CoreMemSet  = 2,
	
	BA_BLTIN_Sys         = 3,
	BA_BLTIN_SysRead     = 4,
	BA_BLTIN_SysWrite    = 5,
	BA_BLTIN_SysOpen     = 6,
	BA_BLTIN_SysClose    = 7,
	BA_BLTIN_SysStat     = 8,
	BA_BLTIN_SysFStat    = 9,
	BA_BLTIN_SysLStat    = 10,
	BA_BLTIN_SysPoll     = 11,
	BA_BLTIN_SysLSeek    = 12,
	BA_BLTIN_SysMMap     = 13,
	BA_BLTIN_SysMProtect = 14,
	BA_BLTIN_SysMUnmap   = 15,
	BA_BLTIN_SysBrk      = 16,
	
	BA_BLTIN__COUNT      = 17,
	BA_BLTIN_FLAG_CNT    = 3, // ceil(BA_BLTIN__COUNT / 8.0)
};

void ba_BltinFlagsSet(u64 flag);
u64 ba_BltinFlagsTest(u64 flag);
void ba_BltinLblSet(u64 bltin, u64 lbl);
u64 ba_BltinLblGet(u64 bltin);
struct ba_Func* ba_IncludeAddFunc(struct ba_Ctr* ctr, u64 line, u64 col, 
	char* funcName);

#endif
