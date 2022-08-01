// See LICENSE for copyright/license information

#include "exitmsg.h"
#include "im.h"

bool isSilenceWarns = 0;
bool isWarnsAsErrs = 0;

bool ba_IsSilenceWarns() {
	return isSilenceWarns;
}

bool ba_IsWarnsAsErrs() {
	return isWarnsAsErrs;
}

void ba_SetSilenceWarns() {
	isSilenceWarns = 1;
}

void ba_SetWarnsAsErrs() {
	isWarnsAsErrs = 1;
}

// "2" because it requires 2 message strings
u8 ba_ExitMsg2(u8 type, char* preMsg, u64 line, u64 col, char* path, 
	char* postMsg) 
{
	if (type != BA_EXIT_ERR && isSilenceWarns) {
		return 0;
	}
	switch (type) {
		case BA_EXIT_ERR:
			fprintf(stderr, "Error");
			break;
		case BA_EXIT_WARN:
			fprintf(stderr, "Warning");
			break;
	}
	fprintf(stderr, ": %s line %llu:%llu in %s%s\n", 
		preMsg, line, col, path, postMsg);
	if (type == BA_EXIT_ERR || (isWarnsAsErrs && type == BA_EXIT_WARN)) {
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

u8 ba_ErrorTknOverflow(char* type, u64 line, u64 col, char* path, u64 max) {
	fprintf(stderr, "Error: encountered %s at %llu:%llu in %s greater than "
		"maximum allowed length (%llu characters)\n", type, line, col, path, max);
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
		"did you mean 'void*'?", line, col, path);
}

u8 ba_ErrorDerefNonPtr(u64 line, u64 col, char* path) {
	return ba_ExitMsg(BA_EXIT_ERR, "dereferencing data which is not castable "
		"to a pointer on", line, col, path);
}

u8 ba_ErrorDerefInvalid(u64 line, u64 col, char* path) {
	return ba_ExitMsg(BA_EXIT_ERR, "dereferencing invalid pointer on", 
		line, col, path);
}

u8 ba_ErrorConvertTypes(u64 line, u64 col, char* path, 
	struct ba_Type expType, struct ba_Type newType) 
{
	fprintf(stderr, "Error: invalid type conversion on line %lld:%lld in %s: "
		"cannot convert expression of type %s to type %s\n", line, col, path, 
		ba_GetTypeStr(expType), ba_GetTypeStr(newType));
	exit(-1);
	return 0;
}

u8 ba_WarnImplicitSignedConversion(u64 line, u64 col, char* path, char* opName) {
	char msg[128] = {0};
	strcat(msg, opName);
	strcat(msg, " of integers of different signedness on");
	ba_ExitMsg2(BA_EXIT_WARN, msg, line, col, path, 
		isWarnsAsErrs ? "" : ", implicitly converted operands to i64");
	return 0;
}

u8 ba_ErrorIMArgCount(u64 args, struct ba_IM* im) {
	fprintf(stderr, "Error: instruction requires %llu arguments: %s\n", 
		args, ba_IMToStr(im));
	exit(-1);
	return 0;
}

u8 ba_ErrorIMArgInvalid(struct ba_IM* im) {
	fprintf(stderr, "Error: invalid set of arguments to instruction %s\n", 
		ba_IMToStr(im));
	exit(-1);
	return 0;
}

