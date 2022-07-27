// See LICENSE for copyright/license information

#ifndef BA__COMMON_TYPES_H
#define BA__COMMON_TYPES_H

struct ba_Type {
	u8 type;
	/* For PTR: ba_Type*, the type pointed to */
	void* extraInfo;
};

struct ba_Str {
	char* str;
	u64 len;
	u64 staticStart;
};

struct ba_ArrExtraInfo {
	struct ba_Type type;
	u64 cnt;
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

#endif
