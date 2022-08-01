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
 * Returns: (rax) program break */
void ba_BltinSysBrk(struct ba_Ctr* ctr) {
	ba_BltinFlagsSet(BA_BLTIN_SysBrk);
	ba_BltinLblSet(BA_BLTIN_SysBrk, ctr->labelCnt);
	++ctr->labelCnt;
	ba_AddIM(ctr, 2, BA_IM_LABEL, ctr->labelCnt-1);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBX); // Store return location in rbx
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBP);
	ba_AddIM(ctr, 3, BA_IM_MOV, BA_IM_RBP, BA_IM_RSP);
	ba_AddIM(ctr, 4, BA_IM_MOV, BA_IM_RAX, BA_IM_IMM, 12);
	ba_AddIM(ctr, 5, BA_IM_MOV, BA_IM_RDI, BA_IM_ADRADD, BA_IM_RBP, 0x08);
	ba_AddIM(ctr, 1, BA_IM_SYSCALL);
	ba_AddIM(ctr, 2, BA_IM_POP, BA_IM_RBP); // Restore rbp
	ba_AddIM(ctr, 2, BA_IM_PUSH, BA_IM_RBX); // Push return location
	ba_AddIM(ctr, 1, BA_IM_RET);
}

// TODO: 13 rt_sigaction, 14 rt_sigprocmask, 15 rt_sigreturn

/* Including all the syscalls */

void ba_IncludeSys(struct ba_Ctr* ctr, u64 line, u64 col) {
	struct ba_Type voidPtr = { BA_TYPE_PTR, ba_MAlloc(sizeof(struct ba_Type)) };
	((struct ba_Type*)voidPtr.extraInfo)->type = BA_TYPE_VOID;
	struct ba_Type u8Ptr = { BA_TYPE_PTR, ba_MAlloc(sizeof(struct ba_Type)) };
	((struct ba_Type*)u8Ptr.extraInfo)->type = BA_TYPE_U8;

	if (!ba_BltinFlagsTest(BA_BLTIN_SysRead)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Read");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysRead(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam()
		};
		*(params[2]) = (struct ba_FuncParam){ // fd (RDI)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.hasDefaultVal = 1,
			.defaultVal = (void*)0, // sys.FD_STDIN
		};
		*(params[1]) = (struct ba_FuncParam){ // count (RDX)
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // count (RDX)
			.type = voidPtr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysRead);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysWrite)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Write");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysWrite(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam()
		};
		*(params[2]) = (struct ba_FuncParam){ // fd (RDI)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.hasDefaultVal = 1,
			.defaultVal = (void*)1, // sys.FD_STDOUT
		};
		*(params[1]) = (struct ba_FuncParam){ // count (RDX)
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // buf (RSI)
			.type = voidPtr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysWrite);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysOpen)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Open");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysOpen(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam()
		};
		*(params[2]) = (struct ba_FuncParam){ // mode (RDX)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.hasDefaultVal = 1,
			.defaultVal = (void*)0,
		};
		*(params[1]) = (struct ba_FuncParam){ // flags (RSI)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // pathname (RDI)
			.type = u8Ptr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysOpen);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysClose)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Close");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysClose(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->firstParam = ba_NewFuncParam(); // fd (RDI)
		func->firstParam->type = (struct ba_Type){ BA_TYPE_I64, 0 };

		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysClose);
		func->doesReturn = 1;
		func->paramCnt = 1;
		func->paramStackSize = 0x8;
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysLSeek)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "LSeek");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysLSeek(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam()
		};
		*(params[2]) = (struct ba_FuncParam){ // whence (RDX)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
		};
		*(params[1]) = (struct ba_FuncParam){ // offset (RSI)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // fd (RDI)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysLSeek);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysMMap)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "MMap");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysMMap(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[6] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam(),
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam(),
		};
		*(params[5]) = (struct ba_FuncParam){ // offset (R9)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.hasDefaultVal = 1,
			.defaultVal = (void*)0,
		};
		*(params[4]) = (struct ba_FuncParam){ // fd (R8)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.hasDefaultVal = 1,
			.defaultVal = (void*)-1,
			.next = params[5],
		};
		*(params[3]) = (struct ba_FuncParam){ // flags (R10)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.next = params[4],
		};
		*(params[2]) = (struct ba_FuncParam){ // prot (RDX)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
			.next = params[3],
		};
		*(params[1]) = (struct ba_FuncParam){ // size (RSI)
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // addr (RDI)
			.type = voidPtr,
			.hasDefaultVal = 1,
			.defaultVal = (void*)0, // NULL
			.next = params[1],
		};
		func->retType = voidPtr;
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysMMap);
		func->doesReturn = 1;
		func->paramCnt = 6;
		func->paramStackSize = 0x30;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysMProtect)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "MProtect");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysMProtect(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[3] = {
			ba_NewFuncParam(), ba_NewFuncParam(), ba_NewFuncParam()
		};
		*(params[2]) = (struct ba_FuncParam){ // prot (RDX)
			.type = (struct ba_Type){ BA_TYPE_I64, 0 },
		};
		*(params[1]) = (struct ba_FuncParam){ // size (RSI)
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
			.next = params[2],
		};
		*(params[0]) = (struct ba_FuncParam){ // addr (RDI)
			.type = voidPtr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysMProtect);
		func->doesReturn = 1;
		func->paramCnt = 3;
		func->paramStackSize = 0x18;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysMUnmap)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "MUnmap");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysMUnmap(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		struct ba_FuncParam* params[2] = {
			ba_NewFuncParam(), ba_NewFuncParam(),
		};
		*(params[1]) = (struct ba_FuncParam){ // size (RSI)
			.type = (struct ba_Type){ BA_TYPE_U64, 0 },
		};
		*(params[0]) = (struct ba_FuncParam){ // addr (RDI)
			.type = voidPtr,
			.next = params[1],
		};
		func->retType = (struct ba_Type){ BA_TYPE_I64, 0 };
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysMUnmap);
		func->doesReturn = 1;
		func->paramCnt = 2;
		func->paramStackSize = 0x10;
		func->firstParam = params[0];
	}
	if (!ba_BltinFlagsTest(BA_BLTIN_SysBrk)) {
		struct ba_Func* func = ba_IncludeAddFunc(ctr, line, col, "Brk");
		struct ba_IM* oldIM = ctr->im;
		ctr->im = func->imBegin;
		ba_BltinSysBrk(ctr);
		func->imEnd = ctr->im;
		ctr->im = oldIM;

		func->firstParam = ba_NewFuncParam(); // addr (RDI)
		func->firstParam->type = voidPtr;
		func->firstParam->hasDefaultVal = 1;
		func->firstParam->defaultVal = (void*)0;

		func->retType = voidPtr;
		func->lblStart = ba_BltinLblGet(BA_BLTIN_SysBrk);
		func->doesReturn = 1;
		func->paramCnt = 1;
		func->paramStackSize = 0x08;
	}
}

