// See LICENSE for copyright/license information

#ifndef BA__TYPES_H
#define BA__TYPES_H

enum {
	BA_TYPE_NONE = 0,

	BA_TYPE_U64 = 0x1,
	BA_TYPE_I64 = 0x2,
	BA_TYPE_U8 = 0x3,
	BA_TYPE_I8 = 0x4,

	BA_TYPE_F64 = 0x5,
	BA_TYPE_F32 = 0x6,
	
	BA_TYPE_TYPE = 0x1000, // ooo meta
};

u8 ba_IsTypeUnsigned(u64 type) {
	return (type == BA_TYPE_U64) || (type == BA_TYPE_U8);
}

u8 ba_IsTypeSigned(u64 type) {
	return (type == BA_TYPE_I64) || (type == BA_TYPE_I8);
}

u8 ba_IsTypeIntegral(u64 type) {
	return ba_IsTypeUnsigned(type) || ba_IsTypeSigned(type);
}

u8 ba_IsTypeNumeric(u64 type) {
	return ba_IsTypeUnsigned(type) || ba_IsTypeSigned(type);
}

u64 ba_GetSizeOfType(u64 type) {
	switch (type) {
		case BA_TYPE_U64:
		case BA_TYPE_I64:
		case BA_TYPE_F64:
			return 8;
		case BA_TYPE_F32:
			return 4;
		case BA_TYPE_U8:
		case BA_TYPE_I8:
			return 1;
	}
	return 0;
}

char* ba_GetTypeStr(u64 type) {
	switch (type) {
		case BA_TYPE_U64:
			return "'u64'";
		case BA_TYPE_I64:
			return "'i64'";
		case BA_TYPE_U8:
			return "'u8'";
		case BA_TYPE_I8:
			return "'i8'";
		case BA_TYPE_F64:
			return "'f64'";
		case BA_TYPE_F32:
			return "'f32'";
	}
	return 0;
}

#endif
