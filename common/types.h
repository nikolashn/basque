// See LICENSE for copyright/license information

#ifndef BA__TYPES_H
#define BA__TYPES_H

struct ba_Type {
	u8 type;
	/* For PTR: ba_Type*, the type pointed to */
	void* extraInfo;
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
	
	BA_TYPE_TYPE = 0xff, // ooo meta
};

bool ba_IsTypeUnsigned(u64 type) {
	return (type == BA_TYPE_U64) || (type == BA_TYPE_U8) || 
		(type == BA_TYPE_BOOL) || (type == BA_TYPE_PTR);
}

bool ba_IsTypeSigned(u64 type) {
	return (type == BA_TYPE_I64) || (type == BA_TYPE_I8);
}

bool ba_IsTypeIntegral(u64 type) {
	return ba_IsTypeUnsigned(type) || ba_IsTypeSigned(type);
}

bool ba_IsTypeNumeric(u64 type) {
	return ba_IsTypeUnsigned(type) || ba_IsTypeSigned(type);
}

u64 ba_GetSizeOfType(struct ba_Type type) {
	switch (type.type) {
		case BA_TYPE_U64:
		case BA_TYPE_I64:
		case BA_TYPE_F64:
		case BA_TYPE_PTR:
			return 8;
		case BA_TYPE_F32:
			return 4;
		case BA_TYPE_U8:
		case BA_TYPE_I8:
		case BA_TYPE_BOOL:
			return 1;
	}
	return 0;
}

bool ba_AreTypesEqual(struct ba_Type a, struct ba_Type b) {
	if (b.type == BA_TYPE_PTR) {
		return ba_AreTypesEqual(*(struct ba_Type*)a.extraInfo, 
			*(struct ba_Type*)b.extraInfo);
	}
	// TODO: FUNC
	return a.type == b.type;
}

char* ba_GetTypeStr(struct ba_Type type) {
	switch (type.type) {
		case BA_TYPE_U64:
			return "'u64'";
		case BA_TYPE_I64:
			return "'i64'";
		case BA_TYPE_U8:
			return "'u8'";
		case BA_TYPE_I8:
			return "'i8'";
		case BA_TYPE_BOOL:
			return "'bool'";
		case BA_TYPE_F64:
			return "'f64'";
		case BA_TYPE_F32:
			return "'f32'";
		case BA_TYPE_TYPE:
			return "type";
		case BA_TYPE_PTR: {
			char* pointedStr = 
				ba_GetTypeStr(*(struct ba_Type*)type.extraInfo);
			u64 pointedStrLen = strlen(pointedStr);
			char* typeStr = malloc(pointedStrLen+2);
			typeStr[pointedStrLen] = '*';
			typeStr[pointedStrLen+1] = 0;
			return typeStr;
		}
	}
	return 0;
}

#endif
