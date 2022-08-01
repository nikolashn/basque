// See LICENSE for copyright/license information

#ifndef BA__COMMON_EXITMSG_H
#define BA__COMMON_EXITMSG_H

#include "types.h"

struct ba_IM; // Forward declaration

enum {
	BA_EXIT_ERR = 0,
	BA_EXIT_WARN = 1,
};

bool ba_IsSilenceWarns();
bool ba_IsWarnsAsErrs();
void ba_SetSilenceWarns();
void ba_SetWarnsAsErrs();
u8 ba_ExitMsg2(u8 type, char* preMsg, u64 line, u64 col, char* path, 
	char* postMsg);
u8 ba_ExitMsg(u8 type, char* msg, u64 line, u64 col, char* path);
u8 ba_ErrorIntLitChar(u64 line, u64 col, char* path);
u8 ba_ErrorIntLitTooLong(u64 line, u64 col, char* path);
u8 ba_ErrorTknOverflow(char* type, u64 line, u64 col, char* path, u64 max);
u8 ba_ErrorIdUndef(char* var, u64 line, u64 col, char* path);
u8 ba_ErrorVarRedef(char* var, u64 line, u64 col, char* path);
u8 ba_ErrorVarVoid(u64 line, u64 col, char* path);
u8 ba_ErrorDerefNonPtr(u64 line, u64 col, char* path);
u8 ba_ErrorDerefInvalid(u64 line, u64 col, char* path);
u8 ba_ErrorConvertTypes(u64 line, u64 col, char* path, 
	struct ba_Type expType, struct ba_Type newType);
u8 ba_WarnImplicitSignedConversion(u64 line, u64 col, char* path, char* opName);
u8 ba_ErrorIMArgCount(u64 args, struct ba_IM* im);
u8 ba_ErrorIMArgInvalid(struct ba_IM* im);

#endif
