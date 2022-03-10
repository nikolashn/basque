// See LICENSE for copyright/license information

#ifndef BA__TYPES_H
#define BA__TYPES_H

struct ba_Type {
	u8 type;
	/* For PTR: ba_Type*, the type pointed to */
	void* extraInfo;
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

// TODO: convert all of these into func(struct ba_Type)->bool

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

bool ba_IsTypeConst(u64 type) {
	return 0; // TODO
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
		case BA_TYPE_DPTR:
			return ba_GetSizeOfType(*(struct ba_Type*)type.extraInfo);
		case BA_TYPE_ARR: {
			struct ba_ArrExtraInfo info = 
				*(struct ba_ArrExtraInfo*)type.extraInfo;
			return ba_GetSizeOfType(info.type) * info.cnt;
		}
	}
	return 0;
}

bool ba_AreTypesEqual(struct ba_Type a, struct ba_Type b) {
	if ((a.type == BA_TYPE_PTR && b.type == BA_TYPE_PTR) || 
		(a.type == BA_TYPE_DPTR && b.type == BA_TYPE_DPTR))
	{
		return ba_AreTypesEqual(*(struct ba_Type*)a.extraInfo, 
			*(struct ba_Type*)b.extraInfo);
	}
	if (a.type == BA_TYPE_ARR && b.type == BA_TYPE_ARR) {
		struct ba_ArrExtraInfo aInfo = *(struct ba_ArrExtraInfo*)a.extraInfo;
		struct ba_ArrExtraInfo bInfo = *(struct ba_ArrExtraInfo*)b.extraInfo;
		return aInfo.cnt == bInfo.cnt && 
			ba_AreTypesEqual(aInfo.type, bInfo.type);
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
			char* pntdStr = ba_GetTypeStr(*(struct ba_Type*)type.extraInfo);
			u64 pntdStrLen = strlen(pntdStr);
			char* typeStr = malloc(pntdStrLen+2);
			strcpy(typeStr, pntdStr);
			typeStr[pntdStrLen] = '*';
			typeStr[pntdStrLen+1] = 0;
			return typeStr;
		}
		case BA_TYPE_DPTR:
			return ba_GetTypeStr(*(struct ba_Type*)type.extraInfo);
		case BA_TYPE_ARR: {
			struct ba_ArrExtraInfo info = 
				*(struct ba_ArrExtraInfo*)type.extraInfo;
			char* pntdTypeStr = ba_GetTypeStr(info.type);
			char sizeStr[20];
			sprintf(sizeStr, "%lld", info.cnt);
			u64 pntdTypeStrLen = strlen(pntdTypeStr);
			u64 sizeStrLen = strlen(sizeStr);
			u64 typeStrSize = pntdTypeStrLen+sizeStrLen+3;
			char* typeStr = malloc(typeStrSize);
			strcpy(typeStr, pntdTypeStr);
			typeStr[pntdTypeStrLen] = '[';
			strcpy(typeStr+pntdTypeStrLen+1, sizeStr);
			typeStr[typeStrSize-2] = ']';
			typeStr[typeStrSize-1] = 0;
			return typeStr;
		}
		// TODO: FUNC
	}
	return 0;
}

#endif
