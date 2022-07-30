// See LICENSE for copyright/license information

#include "../common/func.h"
#include "bltin.h"

/* Wrappers for syscalls */

/* syscall read
 * Params: buf (0x8), count (0x8), fd (0x8)
 * Returns: (rax) no. of bytes read or -1 if err */
void ba_BltinSysRead(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysRead);
	ba_BltinLblSet(BA_BLTIN_SysRead, ctr->labelCnt);
	++ctr->labelCnt;
	
	// --- sys.Read ---
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	// RDI=fd, RSI=buf, RDX=count
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x28);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x20);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall write
 * Params: buf (0x8), count (0x8), fd (0x8)
 * Returns: (rax) no. of bytes written or -1 if err */
void ba_BltinSysWrite(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysWrite);
	ba_BltinLblSet(BA_BLTIN_SysWrite, ctr->labelCnt);
	++ctr->labelCnt;
	
	// --- sys.Write ---
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RDX);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	// RDI=fd, RSI=buf, RDX=count
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x28);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x20);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDX);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RSI);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RDI);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* Including all the syscalls */

struct ba_Func* ba_IncludeSysAddFunc(struct ba_Ctr* ctr, u64 line, u64 col, 
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

void ba_IncludeSys(struct ba_Ctr* ctr, u64 line, u64 col) {
	if (!ba_BltinFlagsTest(BA_BLTIN_SysRead)) {
		struct ba_Func* func = ba_IncludeSysAddFunc(ctr, line, col, "Read");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysRead(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysRead);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;

		struct ba_FuncParam* params[3];

		// buf (RSI)
		params[0] = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		// count (RDX)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_U64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		// fd (RDI)
		params[2] = ba_NewFuncParam();
		params[2]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[2]->hasDefaultVal = 1;
		params[2]->defaultVal = 0; // sys.FD_STDIN
		params[1]->next = params[2];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysWrite)) {
		struct ba_Func* func = ba_IncludeSysAddFunc(ctr, line, col, "Write");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysWrite(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysWrite);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;

		struct ba_FuncParam* params[3];

		// buf (RSI)
		params[0] = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		// count (RDX)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_U64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		// fd (RDI)
		params[2] = ba_NewFuncParam();
		params[2]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[2]->hasDefaultVal = 1;
		params[2]->defaultVal = (void*)1; // sys.FD_STDOUT
		params[1]->next = params[2];
	}
}

