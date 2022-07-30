// See LICENSE for copyright/license information

#ifndef BA__BLTIN_Sys_H
#define BA__BLTIN_Sys_H

#include "../common/ctr.h"

void ba_BltinSysRead(struct ba_Ctr* ctr);
void ba_BltinSysWrite(struct ba_Ctr* ctr);
struct ba_Func* ba_IncludeSysAddFunc(struct ba_Ctr* ctr, u64 line, u64 col, 
	char* funcName);
void ba_IncludeSys(struct ba_Ctr* ctr, u64 line, u64 col);

#endif
