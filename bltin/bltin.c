// See LICENSE for copyright/license information

#include "bltin.h"
#include "../common/ctr.h"
#include "../common/symtable.h"

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

struct ba_Func* ba_IncludeAddFunc(struct ba_Ctr* ctr, u64 line, u64 col, 
	char* funcName) 
{
	// TODO: once named scopes are added, change this
	if (ctr->currScope != ctr->globalST) {
		ba_ExitMsg(BA_EXIT_ERR, "func (from built-in include) can only be "
			"defined in the outer scope,", line, col, ctr->currPath);
	}

	struct ba_STVal* prevFuncIdVal = ba_HTGet(ctr->currScope->ht, funcName);
	if (prevFuncIdVal && (prevFuncIdVal->type.type != BA_TYPE_FUNC || 
		prevFuncIdVal->isInited))
	{
		ba_ErrorVarRedef(funcName, line, col, ctr->currPath);
	}

	struct ba_STVal* funcIdVal = malloc(sizeof(*funcIdVal));
	(!funcIdVal) && ba_ErrorMallocNoMem();
	ba_HTSet(ctr->currScope->ht, funcName, (void*)funcIdVal);

	funcIdVal->scope = ctr->currScope;
	funcIdVal->type.type = BA_TYPE_FUNC;
	funcIdVal->isInited = 1;

	struct ba_Func* func = ba_NewFunc();
	funcIdVal->type.extraInfo = (void*)func;
	
	return func;
}

