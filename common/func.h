// See LICENSE for copyright/license information

#ifndef BA__COMMON_FUNC_H
#define BA__COMMON_FUNC_H

#include "types.h"
#include "im.h"
#include "symtable.h"

struct ba_FuncParam {
	struct ba_Type type;
	struct ba_FuncParam* next;
	void* defaultVal;
	struct ba_STVal* stVal;
	bool hasDefaultVal;
};

struct ba_Func {
	struct ba_Type retType;
	u64 lblStart;
	u64 lblEnd;
	struct ba_SymTable* childScope;
	struct ba_FuncParam* firstParam;
	u64 paramCnt;
	u64 paramStackSize;
	struct ba_IM* imBegin;
	struct ba_IM* imEnd;
	bool isCalled;
	bool doesReturn;
};

struct ba_FuncParam* ba_NewFuncParam();
struct ba_Func* ba_NewFunc();
void ba_DelFunc(struct ba_Func* func);

#endif
