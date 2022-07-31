// See LICENSE for copyright/license information

#include "../common/func.h"
#include "bltin.h"

/* Wrappers for syscalls */

/* syscall read
 * Params: buf (0x8), count (0x8), fd (0x8)
 * Returns: (rax) no. of bytes read */
void ba_BltinSysRead(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysRead);
	ba_BltinLblSet(BA_BLTIN_SysRead, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 3, BA_IM_XOR, BA_IM_RAX, BA_IM_RAX);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall write
 * Params: buf (0x8), count (0x8), fd (0x8)
 * Returns: (rax) no. of bytes written */
void ba_BltinSysWrite(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysWrite);
	ba_BltinLblSet(BA_BLTIN_SysWrite, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 1);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall open
 * Params: filename (0x8), flags (0x8), mode (0x8)
 * Returns: (rax) file descriptor */
void ba_BltinSysOpen(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysOpen);
	ba_BltinLblSet(BA_BLTIN_SysOpen, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 2);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall close
 * Params: filename (0x8)
 * Returns: (rax) 0 on success, -1 on error */
void ba_BltinSysClose(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysClose);
	ba_BltinLblSet(BA_BLTIN_SysClose, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 3);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

// TODO: 4 stat, 5 fstat, 6 lstat, 7 poll

/* syscall lseek
 * Params: fd (0x8), offset (0x8), whence (0x8)
 * Returns: (rax) offset location */
void ba_BltinSysLSeek(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysLSeek);
	ba_BltinLblSet(BA_BLTIN_SysLSeek, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 8);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall mmap
 * Params: addr (0x8), size (0x8), prot (0x8), flags (0x8), fd (0x8), off (0x8)
 * Returns: (rax) pointer to mapped area */
void ba_BltinSysMMap(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysMMap);
	ba_BltinLblSet(BA_BLTIN_SysMMap, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_R10);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_R8);
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_R9);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 9);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x48);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x40);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x38);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_R10, BA_IM_ADRADD, BA_IM_RBP, 0x30);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_R8, BA_IM_ADRADD, BA_IM_RBP, 0x28);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_R9, BA_IM_ADRADD, BA_IM_RBP, 0x20);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_R9);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_R8);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_R10);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall mprotect
 * Params: addr (0x8), size (0x8), prot (0x8)
 * Returns: (rax) offset location */
void ba_BltinSysMProtect(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysMProtect);
	ba_BltinLblSet(BA_BLTIN_SysMProtect, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x18);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDX, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall munmap
 * Params: start (0x8), size (0x8)
 * Returns: (rax) 0 on success, -1 on failure */
void ba_BltinSysMUnmap(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysMUnmap);
	ba_BltinLblSet(BA_BLTIN_SysMUnmap, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 11);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x10);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RSI, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

/* syscall brk
 * Params: start (0x8), size (0x8)
 * Returns: (rax) 0 on success, -1 on failure */
void ba_BltinSysBrk(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysBrk);
	ba_BltinLblSet(BA_BLTIN_SysBrk, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 11);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

// TODO: 13 rt_sigaction, 14 rt_sigprocmask, 15 rt_sigreturn

/* Including all the syscalls */

void ba_IncludeSys(struct ba_Ctr* ctr, u64 line, u64 col) {
	if (!ba_BltinFlagsTest(BA_BLTIN_SysRead)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Read");
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
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Write");
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
	if (!ba_BltinFlagsTest(BA_BLTIN_SysOpen)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Open");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysOpen(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysOpen);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;

		struct ba_FuncParam* params[3];

		// pathname (RDI)
		params[0] = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_U8;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		// flags (RSI)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		// mode (RDX)
		params[2] = ba_NewFuncParam();
		params[2]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[2]->hasDefaultVal = 1;
		params[2]->defaultVal = (void*)0;
		params[1]->next = params[2];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysClose)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Close");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysClose(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysClose);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 1;
		func->paramStackSize = 0x8;

		// fd (RDI)
		func->firstParam = ba_NewFuncParam();
		func->firstParam->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->firstParam->hasDefaultVal = 0;
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysLSeek)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "LSeek");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysLSeek(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysLSeek);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;

		struct ba_FuncParam* params[3];

		// fd (RDI)
		params[0] = ba_NewFuncParam();
		params[0]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		// offset (RSI)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		// whence (RDX)
		params[2] = ba_NewFuncParam();
		params[2]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[2]->hasDefaultVal = 0;
		params[1]->next = params[2];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysMMap)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "MMap");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysMMap(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			func->retType = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysMMap);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 6;
		func->paramStackSize = 0x30;

		struct ba_FuncParam* params[6];

		// addr (RDI)
		params[0] = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 1;
		params[0]->defaultVal = 0; // NULL
		func->firstParam = params[0];

		// size (RSI)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_U64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		// prot (RDX)
		params[2] = ba_NewFuncParam();
		params[2]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[2]->hasDefaultVal = 0;
		params[1]->next = params[2];

		// flags (R10)
		params[3] = ba_NewFuncParam();
		params[3]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[3]->hasDefaultVal = 0;
		params[2]->next = params[3];

		// fd (R9)
		params[4] = ba_NewFuncParam();
		params[4]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[4]->hasDefaultVal = 1;
		params[4]->defaultVal = (void*)-1;
		params[3]->next = params[4];

		// offset (R8)
		params[5] = ba_NewFuncParam();
		params[5]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[5]->hasDefaultVal = 1;
		params[5]->defaultVal = 0;
		params[4]->next = params[5];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysMProtect)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "MProtect");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysMProtect(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysMProtect);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;

		struct ba_FuncParam* params[3];

		// addr (RDI)
		params[0] = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		// size (RSI)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_U64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];

		// prot (RDX)
		params[2] = ba_NewFuncParam();
		params[2]->type = (struct ba_Type){ BA_TYPE_I64, 0 };
		params[2]->hasDefaultVal = 0;
		params[1]->next = params[2];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysMUnmap)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "MUnmap");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysMUnmap(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysMUnmap);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 2;
		func->paramStackSize = 0x10;

		struct ba_FuncParam* params[2];

		// addr (RDI)
		params[0] = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			params[0]->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		params[0]->hasDefaultVal = 0;
		func->firstParam = params[0];

		// size (RSI)
		params[1] = ba_NewFuncParam();
		params[1]->type = (struct ba_Type){ BA_TYPE_U64, 0 };
		params[1]->hasDefaultVal = 0;
		params[0]->next = params[1];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysBrk)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Brk");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysBrk(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysBrk);
		func->isCalled = 0;
		func->doesReturn = 1;
		func->paramCnt = 1;
		func->paramStackSize = 0x08;

		// addr (RDI)
		func->firstParam = ba_NewFuncParam();
		{
			struct ba_Type* fundType = malloc(sizeof(*fundType));
			fundType->type = BA_TYPE_VOID;
			func->firstParam->type = (struct ba_Type){ BA_TYPE_PTR, fundType };
		}
		func->firstParam->hasDefaultVal = 0;
	}
}

