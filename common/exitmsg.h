// See LICENSE for copyright/license information

#ifndef BA__EXITMSG_H
#define BA__EXITMSG_H

#include "types.h"

// --- Forward declaration ---
char* ba_GetTypeStr(struct ba_Type type);
// ---------------------------

// Options

bool ba_IsSilenceWarnings = 0;
bool ba_IsExtraWarnings = 0;
bool ba_IsWarningsAsErrors = 0;

enum {
	BA_EXIT_ERR = 0,
	BA_EXIT_WARN = 1,
	BA_EXIT_EXTRAWARN = 2,
};

// "2" because it requires 2 message strings

u8 ba_ExitMsg2(u8 type, char* preMsg, u64 line, u64 col, char* path, 
	char* postMsg) 
{
	if ((type != BA_EXIT_ERR && ba_IsSilenceWarnings) || 
			(type == BA_EXIT_EXTRAWARN && !ba_IsExtraWarnings)) {
		return 0;
	}
	switch (type) {
		case BA_EXIT_ERR:
			fprintf(stderr, "Error");
			break;
		case BA_EXIT_WARN:
		case BA_EXIT_EXTRAWARN:
			fprintf(stderr, "Warning");
			break;
	}
	fprintf(stderr, ": %s line %llu:%llu in %s%s\n", 
		preMsg, line, col, path, postMsg);
	if (type == BA_EXIT_ERR || (ba_IsWarningsAsErrors && 
		(type == BA_EXIT_WARN || (type == BA_EXIT_EXTRAWARN && 
		ba_IsExtraWarnings)))) 
	{
		exit(-1);
	}
	return 0;
}

// ----- Errors based on the basic function -----

u8 ba_ExitMsg(u8 type, char* msg, u64 line, u64 col, char* path) {
	return ba_ExitMsg2(type, msg, line, col, path, "");
}

u8 ba_ErrorIntLitChar(u64 line, u64 col, char* path) {
	return ba_ExitMsg(BA_EXIT_ERR, "invalid character in integer literal on",
		line, col, path);
}

u8 ba_ErrorIntLitTooLong(u64 line, u64 col, char* path) {
	return ba_ExitMsg2(BA_EXIT_ERR, "integer literal on", line, col, path, 
		" too long to be parsed");
}

// ----- Independent errors ------

u8 ba_ErrorMallocNoMem() {
	fprintf(stderr, "Error: insufficient memory\n");
	exit(-1);
	return 0;
}

u8 ba_ErrorTknOverflow(char* type, u64 line, u64 col, char* path, u64 max) {
	fprintf(stderr, "Error: encountered %s at %llu:%llu in %s greater than "
		"maximum allowed length (%llu characters)\n", type, line, col, path, max);
	exit(-1);
	return 0;
}

u8 ba_ErrorAssignTypes(char* expType, char* var, char* varType, 
	u64 line, u64 col, char* path)
{
	fprintf(stderr, "Error: cannot assign expression of type %s to variable "
		"'%s' of type %s on line %llu:%llu in %s\n", expType, var, varType, 
		line, col, path);
	exit(-1);
	return 0;
}

u8 ba_ErrorIdUndef(char* var, u64 line, u64 col, char* path) {
	fprintf(stderr, "Error: identifier '%s' not defined on line %llu:%llu "
		"in %s\n", var, line, col, path);
	exit(-1);
	return 0;
}

u8 ba_ErrorVarRedef(char* var, u64 line, u64 col, char* path) {
	fprintf(stderr, "Error: redefinition of '%s' on line %llu:%llu in %s\n", 
		var, line, col, path);
	exit(-1);
	return 0;
}

u8 ba_ErrorVarVoid(u64 line, u64 col, char* path) {
	return ba_ExitMsg(BA_EXIT_ERR, "variable cannot be of type 'void', "
		/*"did you mean 'void*'?"*/, line, col, path);
}

u8 ba_ErrorDerefNonPtr(u64 line, u64 col, char* path) {
	return ba_ExitMsg(BA_EXIT_ERR, "dereferencing data which is not castable "
		"to a pointer on", line, col, path);
}

u8 ba_ErrorDerefInvalid(u64 line, u64 col, char* path) {
	return ba_ExitMsg(BA_EXIT_ERR, "dereferencing invalid pointer on", 
		line, col, path);
}

u8 ba_ErrorCastTypes(u64 line, u64 col, char* path, 
	struct ba_Type expType, struct ba_Type newType) 
{
	fprintf(stderr, "Error: invalid type cast on line %lld:%lld in %s: "
		"cannot cast expression of type %s to type %s\n", line, col, path, 
		ba_GetTypeStr(expType), ba_GetTypeStr(newType));
	exit(-1);
	return 0;
}

#endif
