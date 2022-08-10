// See LICENSE for copyright/license information

#ifndef BA__COMMON_TYPES_H
#define BA__COMMON_TYPES_H

#include "common.h"
#include "dynarr.h"

struct ba_Type {
	u8 type;
	void* extraInfo;
};

struct ba_ArrExtraInfo {
	struct ba_Type type;
	u64 cnt;
};

struct ba_Static {
	struct ba_DynArr8* arr;
	u64 offset; // Offset from the start of the static segment
	bool isUsed;
};

struct ba_StaticAddr {
	struct ba_Static* statObj;
	u64 index;
};

struct ba_Str {
	char* str;
	u64 len;
	struct ba_StaticAddr* staticAddr;
};

enum /* u8 */ {
	BA_TYPE_NONE = 0,

	BA_TYPE_U64  = 0x1,
	BA_TYPE_I64  = 0x2,
	BA_TYPE_U8   = 0x3,
	BA_TYPE_I8   = 0x4,
	BA_TYPE_BOOL = 0x5,

	BA_TYPE_F64  = 0x10,
	BA_TYPE_F32  = 0x11,

	BA_TYPE_VOID = 0x20,

	BA_TYPE_FUNC = 0x30,
	BA_TYPE_PTR  = 0x31,
	BA_TYPE_DPTR = 0x32, // a dereferenced pointer
	BA_TYPE_ARR  = 0x33,
	
	BA_TYPE_TYPE = 0xff, // ooo meta
};

bool ba_IsTypeUnsigned(struct ba_Type type);
bool ba_IsTypeSigned(struct ba_Type type);
bool ba_IsTypeInt(struct ba_Type type);
bool ba_IsTypeNum(struct ba_Type type);
u64 ba_GetSizeOfType(struct ba_Type type);
bool ba_AreTypesEqual(struct ba_Type a, struct ba_Type b);
bool ba_IsPermitConvertPtr(struct ba_Type a, struct ba_Type b);
char* ba_GetTypeStr(struct ba_Type type);

#endif
