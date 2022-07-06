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
	u8 hasDefaultVal;
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

struct ba_FuncParam* ba_NewFuncParam() {
	struct ba_FuncParam* param = calloc(1, sizeof(*param));
	if (!param) {
		ba_ErrorMallocNoMem();
	}
	return param;
}

struct ba_Func* ba_NewFunc() {
	struct ba_Func* func = malloc(sizeof(*func));
	func->retType = (struct ba_Type){0};
	func->lblStart = 0;
	func->lblEnd = 0;
	func->childScope = 0;
	func->firstParam = 0;
	func->paramCnt = 0;
	func->paramStackSize = 0;
	func->imBegin = ba_NewIM();
	func->imEnd = 0;
	func->isCalled = 0;
	func->doesReturn = 0;
	return func;
}

void ba_DelFunc(struct ba_Func* func) {
	ba_DelSymTable(func->childScope);
	struct ba_FuncParam* param = func->firstParam;
	while (param) {
		struct ba_FuncParam* next = param->next;
		free(param);
		param = next;
	}
	free(func);
}

#endif
