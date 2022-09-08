// See LICENSE for copyright/license information

#ifndef BA__BLTIN_H
#define BA__BLTIN_H

#include "../common/func.h"
#include "format.h"
#include "core.h"
#include "sys.h"

enum {
	BA_BLTIN_FormatU64ToStr = 0,
	BA_BLTIN_FormatI64ToStr = 1,
	BA_BLTIN_FormatHexToStr = 2,
	BA_BLTIN_FormatOctToStr = 3,
	BA_BLTIN_FormatBinToStr = 4,

	BA_BLTIN_CoreMemCopy    = 5,
	BA_BLTIN_CoreMemSet     = 6,
	
	BA_BLTIN_Sys            = 7,
	BA_BLTIN_SysRead        = 8,
	BA_BLTIN_SysWrite       = 9,
	BA_BLTIN_SysOpen        = 10,
	BA_BLTIN_SysClose       = 11,
	BA_BLTIN_SysStat        = 12,
	BA_BLTIN_SysFStat       = 13,
	BA_BLTIN_SysLStat       = 14,
	BA_BLTIN_SysPoll        = 15,
	BA_BLTIN_SysLSeek       = 16,
	BA_BLTIN_SysMMap        = 17,
	BA_BLTIN_SysMProtect    = 18,
	BA_BLTIN_SysMUnmap      = 19,
	BA_BLTIN_SysBrk         = 20,
	
	BA_BLTIN__COUNT         = 21,
	BA_BLTIN_FLAG_CNT       = 3, // ceil(BA_BLTIN__COUNT / 8.0)
};

void ba_BltinFlagsSet(u64 flag);
u64 ba_BltinFlagsTest(u64 flag);
void ba_BltinLblSet(u64 bltin, u64 lbl);
u64 ba_BltinLblGet(u64 bltin);
struct ba_Func* ba_IncludeAddFunc(struct ba_Ctr* ctr, u64 line, u64 col, 
	char* funcName);

#endif
