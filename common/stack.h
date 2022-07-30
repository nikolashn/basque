// See LICENSE for copyright/license information

#ifndef BA__COMMON_STACK_H
#define BA__COMMON_STACK_H

#include "common.h"

// General stack structure
// The type of items should be known based on which stack is used

struct ba_Stk {
	u64 cap;
	u64 count;
	void** items;
};

struct ba_Stk* ba_NewStk();
void ba_DelStk(struct ba_Stk* stk);
u8 ba_ResizeStk(struct ba_Stk* stk);
void* ba_StkPush(struct ba_Stk* stk, void* item);
void* ba_StkPop(struct ba_Stk* stk);
void* ba_StkTop(struct ba_Stk* stk);

#endif
