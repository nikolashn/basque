// See LICENSE for copyright/license information

#include "func.h"

struct ba_FuncParam* ba_NewFuncParam() {
	struct ba_FuncParam* param = ba_CAlloc(1, sizeof(*param));
	return param;
}
 
struct ba_Func* ba_NewFunc() {
	struct ba_Func* func = ba_MAlloc(sizeof(*func));
	func->retType = (struct ba_Type){0};
	func->lblStart = 0;
	func->lblEnd = 0;
	func->childScope = 0;
	func->firstParam = 0;
	func->paramCnt = 0;
	func->paramStackSize = 0;
	func->contextSize = 0;
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

